#include "UE5_Multi_Shooter/Persist/MosesMatchRecordStorageSubsystem.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

static constexpr int32 Moses_DefaultMaxLoadCount = 50;

bool UMosesMatchRecordStorageSubsystem::SaveMatchRecord_OnResult_Server(const AMosesMatchGameState* MatchGS)
{
	if (!MatchGS || !MatchGS->HasAuthority())
	{
		return false;
	}

	FMosesMatchRecordSnapshot Snapshot;
	if (!BuildSnapshotFromMatchGS_Server(MatchGS, Snapshot))
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save FAIL BuildSnapshot"));
		return false;
	}

	const FString AbsDir = GetRecordsFolderAbsolute();
	if (!EnsureDirectory(AbsDir))
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save FAIL EnsureDirectory Dir=%s"), *AbsDir);
		return false;
	}

	Snapshot.MatchId = MakeMatchIdFromNow_LocalTime();
	Snapshot.Timestamp = MakeTimestampIso_LocalTime();

	const FString FileName = Snapshot.MatchId + TEXT(".json");
	const FString AbsPath = AbsDir / FileName;

	FString Json;
	if (!SnapshotToJson(Snapshot, Json))
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save FAIL JsonSerialize MatchId=%s"), *Snapshot.MatchId);
		return false;
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save Start MatchId=%s Players=%d Path=%s"),
		*Snapshot.MatchId, Snapshot.Players.Num(), *AbsPath);

	if (!SaveStringToFile(AbsPath, Json))
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save FAIL WriteFile Path=%s"), *AbsPath);
		return false;
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Save OK Path=%s"), *AbsPath);
	return true;
}

bool UMosesMatchRecordStorageSubsystem::LoadRecordSummaries_Server(
	const FString& RequesterPersistentId,
	const FString& RequesterNickname,
	const FString& NicknameFilterOptional,
	int32 MaxCount,
	TArray<FMosesMatchRecordSummary>& OutSummaries) const
{
	OutSummaries.Reset();

	MaxCount = (MaxCount > 0) ? MaxCount : Moses_DefaultMaxLoadCount;

	const FString AbsDir = GetRecordsFolderAbsolute();
	TArray<FString> Files;
	ScanJsonFiles(AbsDir, Files);

	// 최신순: 파일명에 시간이 들어간다는 전제( Match_YYYY-MM-DD_HH-MM-SS.json )
	Files.Sort([](const FString& A, const FString& B)
		{
			return A > B;
		});

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Load Start Folder=%s Files=%d Filter='%s'"),
		*AbsDir, Files.Num(), *NicknameFilterOptional);

	int32 Added = 0;

	for (const FString& AbsPath : Files)
	{
		if (Added >= MaxCount)
		{
			break;
		}

		FString Json;
		if (!LoadFileToString(AbsPath, Json))
		{
			UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Load SKIP (ReadFail) File=%s"), *AbsPath);
			continue;
		}

		FMosesMatchRecordSnapshot Snapshot;
		if (!JsonToSnapshot(Json, Snapshot))
		{
			UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Load SKIP (ParseFail) File=%s"), *AbsPath);
			continue;
		}

		// 닉네임 필터(선택): “참가자 중 해당 닉이 있으면 포함”
		if (!NicknameFilterOptional.IsEmpty())
		{
			bool bHas = false;
			for (const FMosesMatchRecordPlayerRow& Row : Snapshot.Players)
			{
				if (Row.Nickname.Equals(NicknameFilterOptional, ESearchCase::IgnoreCase))
				{
					bHas = true;
					break;
				}
			}
			if (!bHas)
			{
				continue;
			}
		}

		FMosesMatchRecordSummary S;
		S.MatchId = Snapshot.MatchId;
		S.Timestamp = Snapshot.Timestamp;
		S.MapName = Snapshot.MapName;
		S.WinnerNickname = Snapshot.WinnerNickname;
		S.ResultReason = Snapshot.ResultReason;
		S.MyResult = DecideMyResult(Snapshot, RequesterPersistentId);

		FMosesMatchRecordPlayerRow MyRow;
		if (FindMyRow(Snapshot, RequesterPersistentId, MyRow))
		{
			S.Captures = MyRow.Captures;
			S.PvPKills = MyRow.PvPKills;
			S.ZombieKills = MyRow.ZombieKills;
			S.Headshots = MyRow.Headshots;
			S.Deaths = MyRow.Deaths;
		}

		OutSummaries.Add(S);
		Added++;
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERSIST][SV] Load OK Records=%d RequesterNick=%s Pid=%s"),
		OutSummaries.Num(), *RequesterNickname, *RequesterPersistentId);

	return true;
}

// ---------------------------------
// Path
// ---------------------------------

FString UMosesMatchRecordStorageSubsystem::GetRecordsFolderRelative()
{
	return TEXT("MatchRecords");
}

FString UMosesMatchRecordStorageSubsystem::GetRecordsFolderAbsolute() const
{
	return FPaths::ProjectSavedDir() / GetRecordsFolderRelative();
}

// ---------------------------------
// Snapshot build
// ---------------------------------

bool UMosesMatchRecordStorageSubsystem::BuildSnapshotFromMatchGS_Server(const AMosesMatchGameState* MatchGS, FMosesMatchRecordSnapshot& OutSnapshot) const
{
	if (!MatchGS || !MatchGS->HasAuthority())
	{
		return false;
	}

	OutSnapshot = FMosesMatchRecordSnapshot();
	OutSnapshot.MapName = MatchGS->GetWorld() ? MatchGS->GetWorld()->GetMapName() : TEXT("Unknown");

	// ResultState에서 Winner/Draw/Reason 읽기
	const FMosesMatchResultState& RS = MatchGS->GetResultState();
	OutSnapshot.bIsDraw = RS.bIsDraw;

	// ✅ [FIX] Persist MVP: OnlineSubsystem 사용 금지 → PersistentId 문자열로 저장
	OutSnapshot.WinnerPersistentId = RS.WinnerPersistentId;
	OutSnapshot.WinnerNickname = RS.WinnerNickname;
	OutSnapshot.ResultReason = RS.ResultReason;

	// 참가자 스탯은 PlayerState(SSOT)에서
	const AGameStateBase* GS = MatchGS->GetWorld() ? MatchGS->GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return false;
	}

	for (APlayerState* Raw : GS->PlayerArray)
	{
		const AMosesPlayerState* PS = Cast<AMosesPlayerState>(Raw);
		if (!PS)
		{
			continue;
		}

		FMosesMatchRecordPlayerRow Row;
		Row.PersistentId = PS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens);
		Row.Nickname = PS->GetPlayerNickName();

		Row.Captures = PS->GetCaptures();
		Row.PvPKills = PS->GetPvPKills();
		Row.ZombieKills = PS->GetZombieKills();
		Row.Headshots = PS->GetHeadshots();
		Row.Deaths = PS->GetDeaths();

		OutSnapshot.Players.Add(Row);
	}

	return true;
}

FString UMosesMatchRecordStorageSubsystem::MakeMatchIdFromNow_LocalTime()
{
	const FDateTime Now = FDateTime::Now();
	return FString::Printf(TEXT("Match_%04d-%02d-%02d_%02d-%02d-%02d"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond());
}

FString UMosesMatchRecordStorageSubsystem::MakeTimestampIso_LocalTime()
{
	// 포폴 증거용: 사람이 읽기 쉬운 로컬 시간
	const FDateTime Now = FDateTime::Now();
	return Now.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
}

// ---------------------------------
// Summary helpers
// ---------------------------------

EMosesRecordResultType UMosesMatchRecordStorageSubsystem::DecideMyResult(const FMosesMatchRecordSnapshot& Snapshot, const FString& MyPersistentId)
{
	if (Snapshot.bIsDraw)
	{
		return EMosesRecordResultType::Draw;
	}

	if (!Snapshot.WinnerPersistentId.IsEmpty() && Snapshot.WinnerPersistentId == MyPersistentId)
	{
		return EMosesRecordResultType::Victory;
	}

	return EMosesRecordResultType::Lose;
}

bool UMosesMatchRecordStorageSubsystem::FindMyRow(const FMosesMatchRecordSnapshot& Snapshot, const FString& MyPersistentId, FMosesMatchRecordPlayerRow& OutRow)
{
	for (const FMosesMatchRecordPlayerRow& Row : Snapshot.Players)
	{
		if (Row.PersistentId == MyPersistentId)
		{
			OutRow = Row;
			return true;
		}
	}
	return false;
}

// ---------------------------------
// File I/O
// ---------------------------------

bool UMosesMatchRecordStorageSubsystem::EnsureDirectory(const FString& AbsDir)
{
	IFileManager& FM = IFileManager::Get();
	if (FM.DirectoryExists(*AbsDir))
	{
		return true;
	}
	return FM.MakeDirectory(*AbsDir, true);
}

bool UMosesMatchRecordStorageSubsystem::SaveStringToFile(const FString& AbsPath, const FString& Text)
{
	return FFileHelper::SaveStringToFile(Text, *AbsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UMosesMatchRecordStorageSubsystem::LoadFileToString(const FString& AbsPath, FString& OutText)
{
	return FFileHelper::LoadFileToString(OutText, *AbsPath);
}

void UMosesMatchRecordStorageSubsystem::ScanJsonFiles(const FString& AbsDir, TArray<FString>& OutAbsFiles)
{
	OutAbsFiles.Reset();

	IFileManager& FM = IFileManager::Get();
	if (!FM.DirectoryExists(*AbsDir))
	{
		return;
	}

	// AbsDir/*.json
	TArray<FString> Found;
	FM.FindFiles(Found, *(AbsDir / TEXT("*.json")), true, false);

	for (const FString& NameOnly : Found)
	{
		OutAbsFiles.Add(AbsDir / NameOnly);
	}
}

// ---------------------------------
// Json serialize/deserialize (간단 수동)
// ---------------------------------

bool UMosesMatchRecordStorageSubsystem::SnapshotToJson(const FMosesMatchRecordSnapshot& Snapshot, FString& OutJson)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	Root->SetStringField(TEXT("MatchId"), Snapshot.MatchId);
	Root->SetStringField(TEXT("Timestamp"), Snapshot.Timestamp);
	Root->SetStringField(TEXT("MapName"), Snapshot.MapName);

	Root->SetBoolField(TEXT("bIsDraw"), Snapshot.bIsDraw);
	Root->SetStringField(TEXT("WinnerPersistentId"), Snapshot.WinnerPersistentId);
	Root->SetStringField(TEXT("WinnerNickname"), Snapshot.WinnerNickname);
	Root->SetStringField(TEXT("ResultReason"), Snapshot.ResultReason);

	TArray<TSharedPtr<FJsonValue>> PlayersJson;

	for (const FMosesMatchRecordPlayerRow& P : Snapshot.Players)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("PersistentId"), P.PersistentId);
		O->SetStringField(TEXT("Nickname"), P.Nickname);

		O->SetNumberField(TEXT("Captures"), P.Captures);
		O->SetNumberField(TEXT("PvPKills"), P.PvPKills);
		O->SetNumberField(TEXT("ZombieKills"), P.ZombieKills);
		O->SetNumberField(TEXT("Headshots"), P.Headshots);
		O->SetNumberField(TEXT("Deaths"), P.Deaths);

		PlayersJson.Add(MakeShared<FJsonValueObject>(O));
	}

	Root->SetArrayField(TEXT("Players"), PlayersJson);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer);
}

bool UMosesMatchRecordStorageSubsystem::JsonToSnapshot(const FString& Json, FMosesMatchRecordSnapshot& OutSnapshot)
{
	OutSnapshot = FMosesMatchRecordSnapshot();

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	Root->TryGetStringField(TEXT("MatchId"), OutSnapshot.MatchId);
	Root->TryGetStringField(TEXT("Timestamp"), OutSnapshot.Timestamp);
	Root->TryGetStringField(TEXT("MapName"), OutSnapshot.MapName);

	Root->TryGetBoolField(TEXT("bIsDraw"), OutSnapshot.bIsDraw);
	Root->TryGetStringField(TEXT("WinnerPersistentId"), OutSnapshot.WinnerPersistentId);
	Root->TryGetStringField(TEXT("WinnerNickname"), OutSnapshot.WinnerNickname);
	Root->TryGetStringField(TEXT("ResultReason"), OutSnapshot.ResultReason);

	const TArray<TSharedPtr<FJsonValue>>* PlayersArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("Players"), PlayersArr) || !PlayersArr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& V : *PlayersArr)
	{
		const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
		if (!O.IsValid())
		{
			continue;
		}

		FMosesMatchRecordPlayerRow P;
		O->TryGetStringField(TEXT("PersistentId"), P.PersistentId);
		O->TryGetStringField(TEXT("Nickname"), P.Nickname);

		double Num = 0.0;
		if (O->TryGetNumberField(TEXT("Captures"), Num)) { P.Captures = (int32)Num; }
		if (O->TryGetNumberField(TEXT("PvPKills"), Num)) { P.PvPKills = (int32)Num; }
		if (O->TryGetNumberField(TEXT("ZombieKills"), Num)) { P.ZombieKills = (int32)Num; }
		if (O->TryGetNumberField(TEXT("Headshots"), Num)) { P.Headshots = (int32)Num; }
		if (O->TryGetNumberField(TEXT("Deaths"), Num)) { P.Deaths = (int32)Num; }

		OutSnapshot.Players.Add(P);
	}

	return true;
}