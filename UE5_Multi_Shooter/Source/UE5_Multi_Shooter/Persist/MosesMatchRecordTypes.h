#pragma once

#include "CoreMinimal.h"
#include "MosesMatchRecordTypes.generated.h"

// ================================
// ResultType (UI 표기용)
// ================================
UENUM(BlueprintType)
enum class EMosesRecordResultType : uint8
{
	Victory UMETA(DisplayName="VICTORY"),
	Lose    UMETA(DisplayName="LOSE"),
	Draw    UMETA(DisplayName="DRAW"),
};

// ================================
// Player Row (저장 본문)
// ================================
USTRUCT(BlueprintType)
struct FMosesMatchRecordPlayerRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString PersistentId;        // FGuid string (DigitsWithHyphens)

	UPROPERTY(BlueprintReadOnly)
	FString Nickname;

	UPROPERTY(BlueprintReadOnly)
	int32 Captures = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PvPKills = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ZombieKills = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Headshots = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Deaths = 0;
};

// ================================
// Snapshot (저장 원본)
// ================================
USTRUCT(BlueprintType)
struct FMosesMatchRecordSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString MatchId;             // "Match_YYYY-MM-DD_HH-MM-SS" (파일명과 동일)

	UPROPERTY(BlueprintReadOnly)
	FString Timestamp;           // ISO or file time string

	UPROPERTY(BlueprintReadOnly)
	FString MapName;

	UPROPERTY(BlueprintReadOnly)
	TArray<FMosesMatchRecordPlayerRow> Players;

	UPROPERTY(BlueprintReadOnly)
	bool bIsDraw = false;

	UPROPERTY(BlueprintReadOnly)
	FString WinnerPersistentId;

	UPROPERTY(BlueprintReadOnly)
	FString WinnerNickname;

	UPROPERTY(BlueprintReadOnly)
	FString ResultReason;        // "Captures/PvPKills/ZombieKills/Headshots/Draw"
};

// ================================
// Summary (로비 ListView용 경량)
// ================================
USTRUCT(BlueprintType)
struct FMosesMatchRecordSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString MatchId;

	UPROPERTY(BlueprintReadOnly)
	FString Timestamp;

	UPROPERTY(BlueprintReadOnly)
	FString MapName;

	UPROPERTY(BlueprintReadOnly)
	EMosesRecordResultType MyResult = EMosesRecordResultType::Lose;

	UPROPERTY(BlueprintReadOnly)
	FString WinnerNickname;

	UPROPERTY(BlueprintReadOnly)
	FString ResultReason;

	// “내 스탯”
	UPROPERTY(BlueprintReadOnly)
	int32 Captures = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PvPKills = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ZombieKills = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Headshots = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Deaths = 0;
};

// ================================
// ListView Item Object (BP 편의)
// ================================
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesMatchRecordSummaryObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	FMosesMatchRecordSummary Data;
};
