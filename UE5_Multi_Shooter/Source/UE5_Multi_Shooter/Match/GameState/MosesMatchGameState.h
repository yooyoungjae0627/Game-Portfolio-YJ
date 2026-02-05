// ============================================================================
// MosesMatchGameState.h (FULL)
// - Match 전용 GameState (Phase / RemainingSeconds / Announcement / ResultState)
// - 부모: AMosesGameState (ExperienceManagerComponent 공통 보장)
// - 정책:
//   - 서버만 변경(Server Authority 100%)
//   - 클라는 RepNotify -> Delegate로 HUD 갱신 (Tick/Binding 금지)
// - PushModel 대응: 서버에서 값 변경 시 Dirty 마킹/ForceNetUpdate를 사용한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesGameState.h"
#include "UE5_Multi_Shooter/Match/MosesMatchTypes.h"
#include "UE5_Multi_Shooter/Match/MosesMatchPhase.h"

#include "MosesMatchGameState.generated.h"

class USoundBase; // ✅ KillFeed 시그니처

// HUD 갱신: RepNotify -> Delegate
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesMatchTimeChangedNative, int32 /*RemainingSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesMatchPhaseChangedNative, EMosesMatchPhase /*Phase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesAnnouncementChangedNative, const FMosesAnnouncementState& /*State*/);

// [MOD] Result
UENUM(BlueprintType)
enum class EMosesResultReason : uint8
{
	None		UMETA(DisplayName = "None"),
	Captures	UMETA(DisplayName = "Captures"),
	PvPKills	UMETA(DisplayName = "PvP Kills"),
	ZombieKills	UMETA(DisplayName = "Zombie Kills"),
	Headshots	UMETA(DisplayName = "Headshots"),
	Draw		UMETA(DisplayName = "Draw"),
};

USTRUCT(BlueprintType)
struct FMosesMatchResultState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bIsResult = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsDraw = false;

	/** Winner UniqueId.ToString() */
	UPROPERTY(BlueprintReadOnly)
	FString WinnerPlayerId;

	UPROPERTY(BlueprintReadOnly)
	EMosesResultReason Reason = EMosesResultReason::None;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesResultStateChangedNative, const FMosesMatchResultState& /*State*/); // [MOD]

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesMatchGameState : public AMosesGameState
{
	GENERATED_BODY()

public:
	AMosesMatchGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// -------------------------------------------------------------------------
	// Delegates (HUD bind)
	// -------------------------------------------------------------------------
	FOnMosesMatchTimeChangedNative OnMatchTimeChanged;
	FOnMosesMatchPhaseChangedNative OnMatchPhaseChanged;
	FOnMosesAnnouncementChangedNative OnAnnouncementChanged;

	// [MOD]
	FOnMosesResultStateChangedNative OnResultStateChanged;

public:
	// -------------------------------------------------------------------------
	// Getters
	// -------------------------------------------------------------------------
	int32 GetRemainingSeconds() const { return RemainingSeconds; }
	EMosesMatchPhase GetMatchPhase() const { return MatchPhase; }
	const FMosesAnnouncementState& GetAnnouncementState() const { return AnnouncementState; }

	// [MOD]
	const FMosesMatchResultState& GetResultState() const { return ResultState; }

	// [MOD] ResultPhase 여부 (서버 Guard 기준)
	bool IsResultPhase() const
	{
		return (MatchPhase == EMosesMatchPhase::Result) || ResultState.bIsResult;
	}

public:
	// -------------------------------------------------------------------------
	// Server API (Authority Only)
	// -------------------------------------------------------------------------
	void ServerSetRemainingSeconds(int32 NewSeconds);
	void ServerSetMatchPhase(EMosesMatchPhase NewPhase);

	void ServerStartAnnouncementText(const FText& InText, int32 DurationSeconds);
	void ServerStartAnnouncementCountdown(const FText& PrefixText, int32 CountdownFromSeconds);
	void ServerStopAnnouncement();

	void ServerStartMatchTimer(int32 TotalSeconds);
	void ServerStopMatchTimer();

	// [MOD] Result state replicate
	void ServerSetResultState(const FMosesMatchResultState& NewState);

public:
	// -------------------------------------------------------------------------
	// Compatibility Wrappers
	// -------------------------------------------------------------------------
	// ===================== Announcement =====================
	/** 코어 버전 (실제 구현) */
	void ServerPushAnnouncement(const FText& Msg, float DurationSeconds = 1.0f);

	/** 기존 호출부 호환: FString */
	void ServerPushAnnouncement(const FString& Msg, float DurationSeconds = 1.0f);

	// ===================== KillFeed =====================
	/** 코어 버전 (실제 구현) */
	void ServerPushKillFeed(const FText& Msg, bool bHeadshot, USoundBase* HeadshotSound);

	/** 단순 메시지 버전 (Headshot=false) */
	void ServerPushKillFeed(const FText& Msg);

	/** 기존 호출부 호환: FString */
	void ServerPushKillFeed(const FString& Msg);

private:
	// -------------------------------------------------------------------------
	// RepNotifies
	// -------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_RemainingSeconds();

	UFUNCTION()
	void OnRep_MatchPhase();

	UFUNCTION()
	void OnRep_AnnouncementState();

	// [MOD]
	UFUNCTION()
	void OnRep_ResultState();

private:
	// -------------------------------------------------------------------------
	// Server tick (1초)
	// -------------------------------------------------------------------------
	void ServerTick_1s();

private:
	// -------------------------------------------------------------------------
	// Replicated
	// -------------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_RemainingSeconds)
	int32 RemainingSeconds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EMosesMatchPhase MatchPhase = EMosesMatchPhase::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_AnnouncementState)
	FMosesAnnouncementState AnnouncementState;

	// [MOD]
	UPROPERTY(ReplicatedUsing = OnRep_ResultState)
	FMosesMatchResultState ResultState;

private:
	// -------------------------------------------------------------------------
	// Server-only
	// -------------------------------------------------------------------------
	FTimerHandle MatchTimerHandle;
	FText ServerAnnouncePrefixText;
};
