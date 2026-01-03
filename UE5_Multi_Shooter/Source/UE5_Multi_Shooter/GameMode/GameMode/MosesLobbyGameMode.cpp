#include "MosesLobbyGameMode.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	// SeamlessTravel을 쓸지 정책화
	PlayerControllerClass = AMosesPlayerController::StaticClass();
	PlayerStateClass = AMosesPlayerState::StaticClass();
	GameStateClass = AMosesLobbyGameState::StaticClass();
	DefaultPawnClass = nullptr; // 로비면 Pawn 안 써도 됨

	bUseSeamlessTravel = bUseSeamlessTravelToMatch;
}

void AMosesLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] BeginPlay Seamless=%d TravelURL=%s"),
		bUseSeamlessTravel ? 1 : 0, *MatchMapTravelURL);
}

void AMosesLobbyGameMode::HandleStartGameRequest(AMosesPlayerController* RequestPC)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("HandleStartGameRequest"));
	if (!LGS)
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(RequestPC, TEXT("HandleStartGameRequest"));
	if (!PS)
	{
		return;
	}

	// ✅ 서버 최종 검증: "Host인지" + "AllReady인지"
	FString FailReason;
	const bool bOk = LGS->Server_CanStartGame(PS, FailReason);
	if (!bOk)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT Reason=%s HostPid=%s"),
			*FailReason, *PS->GetPersistentId().ToString());
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame ACCEPT HostPid=%s -> Travel"),
		*PS->GetPersistentId().ToString());

	DoServerTravelToMatch();
}

void AMosesLobbyGameMode::TravelToMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch (Debug/Exec)"));
	DoServerTravelToMatch();
}

void AMosesLobbyGameMode::HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId)
{
	// ✅ 서버 전용 게이트
	if (!HasAuthority())
	{
		return;
	}

	if (!RequestPC)
	{
		return;
	}

	AMosesPlayerState* PS = RequestPC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (No PS) PC=%s"), *GetNameSafe(RequestPC));
		return;
	}

	// Phase0 최소 검증
	if (CharacterId.IsNone())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (None CharacterId) Pid=%s"),
			*PS->GetPersistentId().ToString());
		return;
	}

	const int32 SelectedId = ResolveCharacterId(CharacterId);
	if (SelectedId < 0)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (Invalid CharacterId=%s) Pid=%s"),
			*CharacterId.ToString(),
			*PS->GetPersistentId().ToString());
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[CharSel][SV][GM] ACCEPT Pid=%s CharacterId=%s -> SelectedId=%d"),
		*PS->GetPersistentId().ToString(),
		*CharacterId.ToString(),
		SelectedId);

	// ✅ 서버가 정답 확정(단일진실: PlayerState)
	PS->ServerSetSelectedCharacterId(SelectedId);

	// (선택) DoD 로그
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterSelectCharacter"));
}

AMosesLobbyGameState* AMosesLobbyGameMode::GetLobbyGameStateChecked_Log(const TCHAR* Caller) const
{
	UWorld* World = GetWorld();
	AMosesLobbyGameState* LGS = World ? World->GetGameState<AMosesLobbyGameState>() : nullptr;

	if (!LGS)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[%s] FAIL (NoLobbyGameState)"), Caller);
	}

	return LGS;
}

AMosesPlayerState* AMosesLobbyGameMode::GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const
{
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[%s] FAIL (NullPC)"), Caller);
		return nullptr;
	}

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[%s] FAIL (NoPlayerState) PC=%s"), Caller, *GetNameSafe(PC));
		return nullptr;
	}

	return PS;
}

void AMosesLobbyGameMode::DoServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// SeamlessTravel을 사용할지 다시 동기화(에디터에서 값 바뀌어도 안전)
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s (Seamless=%d)"),
		*MatchMapTravelURL, bUseSeamlessTravel ? 1 : 0);

	World->ServerTravel(MatchMapTravelURL, /*bAbsolute*/false);
}

int32 AMosesLobbyGameMode::ResolveCharacterId(const FName CharacterId) const
{
	if (!CharacterCatalog)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharSel][SV][GM] FAIL (CharacterCatalog NULL)"));
		return -1;
	}

	const TArray<FMSCharacterEntry>& Entries = CharacterCatalog->GetEntries();

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].CharacterId == CharacterId)
		{
			return i; // Phase0 정책: 카탈로그 인덱스를 SelectedCharacterId로 사용
		}
	}

	return -1;
}
