#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "UE5_Multi_Shooter/Persist/MosesMatchRecordTypes.h"

#include "MosesMatchRecordStorageSubsystem.generated.h"

class AMosesMatchGameState;
class AMosesPlayerController;

/**
 * UMosesMatchRecordStorageSubsystem
 * - 서버 권위 저장/로드 담당 (Saved/MatchRecords)
 * - Result 진입 순간 1회 저장(Write)
 * - 로비 요청 시 목록 로드(Read) 후 ClientRPC로 전달
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesMatchRecordStorageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// -------------------------------
	// Write (Server only)
	// -------------------------------
	bool SaveMatchRecord_OnResult_Server(const AMosesMatchGameState* MatchGS);

	// -------------------------------
	// Read (Server only)
	// -------------------------------
	bool LoadRecordSummaries_Server(
		const FString& RequesterPersistentId,
		const FString& RequesterNickname,
		const FString& NicknameFilterOptional,
		int32 MaxCount,
		TArray<FMosesMatchRecordSummary>& OutSummaries) const;

	// -------------------------------
	// Path helpers
	// -------------------------------
	FString GetRecordsFolderAbsolute() const;
	static FString GetRecordsFolderRelative(); // "MatchRecords"

private:
	// -------------------------------
	// Snapshot build
	// -------------------------------
	bool BuildSnapshotFromMatchGS_Server(const AMosesMatchGameState* MatchGS, FMosesMatchRecordSnapshot& OutSnapshot) const;
	static FString MakeMatchIdFromNow_LocalTime();
	static FString MakeTimestampIso_LocalTime();

	// -------------------------------
	// Summary build
	// -------------------------------
	static EMosesRecordResultType DecideMyResult(const FMosesMatchRecordSnapshot& Snapshot, const FString& MyPersistentId);
	static bool FindMyRow(const FMosesMatchRecordSnapshot& Snapshot, const FString& MyPersistentId, FMosesMatchRecordPlayerRow& OutRow);

	// -------------------------------
	// File I/O
	// -------------------------------
	static bool EnsureDirectory(const FString& AbsDir);
	static bool SaveStringToFile(const FString& AbsPath, const FString& Text);
	static bool LoadFileToString(const FString& AbsPath, FString& OutText);
	static void ScanJsonFiles(const FString& AbsDir, TArray<FString>& OutAbsFiles);

	// -------------------------------
	// Json
	// -------------------------------
	static bool SnapshotToJson(const FMosesMatchRecordSnapshot& Snapshot, FString& OutJson);
	static bool JsonToSnapshot(const FString& Json, FMosesMatchRecordSnapshot& OutSnapshot);

	// -------------------------------
	// Parse helpers
	// -------------------------------
	static FString ExtractFileNameNoExt(const FString& AbsPath);
};
