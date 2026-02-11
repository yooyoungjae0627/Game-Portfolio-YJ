// ============================================================================
// UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h  (CLEANED)
// - Match용 GameState
// - RepNotify -> Native Delegate 로 HUD 갱신 (Tick/Binding 금지)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesGameState.h"
#include "UE5_Multi_Shooter/Match/MosesMatchTypes.h"
#include "UE5_Multi_Shooter/Match/MosesMatchPhase.h"

#include "MosesMatchGameState.generated.h"

class USoundBase;

// HUD 갱신: RepNotify -> Delegate
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesMatchTimeChangedNative, int32 /*RemainingSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesMatchPhaseChangedNative, EMosesMatchPhase /*Phase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesAnnouncementChangedNative, const FMosesAnnouncementState& /*State*/);

UENUM(BlueprintType)
enum class EMosesResultReason : uint8
{
	None,
	Captures,
	PvPKills,
	ZombieKills,
	Headshots,
	Draw,
};

USTRUCT(BlueprintType)
struct FMosesMatchResultState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bIsResult = false;
	UPROPERTY(BlueprintReadOnly) bool bIsDraw = false;

	UPROPERTY(BlueprintReadOnly) FString WinnerPersistentId;
	UPROPERTY(BlueprintReadOnly) FString WinnerNickname;
	UPROPERTY(BlueprintReadOnly) FString ResultReason;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesResultStateChangedNative, const FMosesMatchResultState& /*State*/);

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
	// =========================================================================
	// Delegates (HUD bind)
	// =========================================================================
	FOnMosesMatchTimeChangedNative OnMatchTimeChanged;
	FOnMosesMatchPhaseChangedNative OnMatchPhaseChanged;
	FOnMosesAnnouncementChangedNative OnAnnouncementChanged;
	FOnMosesResultStateChangedNative OnResultStateChanged;

	// =========================================================================
	// Read-only getters
	// =========================================================================
	int32 GetRemainingSeconds() const { return RemainingSeconds; }
	EMosesMatchPhase GetMatchPhase() const { return MatchPhase; }
	const FMosesAnnouncementState& GetAnnouncementState() const { return AnnouncementState; }
	const FMosesMatchResultState& GetResultState() const { return ResultState; }

	bool IsResultPhase() const
	{
		return (MatchPhase == EMosesMatchPhase::Result) || ResultState.bIsResult;
	}

public:
	// =========================================================================
	// Server API (Authority Only)
	// =========================================================================
	void ServerSetRemainingSeconds(int32 NewSeconds);
	void ServerSetMatchPhase(EMosesMatchPhase NewPhase);

	void ServerStartAnnouncementText(const FText& InText, int32 DurationSeconds);
	void ServerStartAnnouncementCountdown(const FText& PrefixText, int32 CountdownFromSeconds);
	void ServerStopAnnouncement();

	void ServerSetCountdownAnnouncement_External(int32 RemainingSec);

	void ServerStartMatchTimer(int32 TotalSeconds);
	void ServerStopMatchTimer();

	void ServerSetResultState(const FMosesMatchResultState& NewState);

	// Compatibility Wrappers
	void ServerPushAnnouncement(const FText& Msg, float DurationSeconds = 1.0f);
	void ServerPushAnnouncement(const FString& Msg, float DurationSeconds = 1.0f);

	void ServerPushKillFeed(const FText& Msg, bool bHeadshot, USoundBase* HeadshotSound);
	void ServerPushKillFeed(const FText& Msg);
	void ServerPushKillFeed(const FString& Msg);

private:
	// =========================================================================
	// RepNotifies
	// =========================================================================
	UFUNCTION() void OnRep_RemainingSeconds();
	UFUNCTION() void OnRep_MatchPhase();
	UFUNCTION() void OnRep_AnnouncementState();
	UFUNCTION() void OnRep_ResultState();

private:
	// =========================================================================
	// Server tick (1 sec timer)
	// =========================================================================
	void ServerTick_1s();

	void BroadcastAnnouncementLocal_Server();
	void BroadcastMatchTimeLocal_Server();
	void BroadcastMatchPhaseLocal_Server();
	void BroadcastResultLocal_Server();

private:
	// =========================================================================
	// Replicated state
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_RemainingSeconds)
	int32 RemainingSeconds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EMosesMatchPhase MatchPhase = EMosesMatchPhase::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_AnnouncementState)
	FMosesAnnouncementState AnnouncementState;

	UPROPERTY(ReplicatedUsing = OnRep_ResultState)
	FMosesMatchResultState ResultState;

private:
	// =========================================================================
	// Server-only runtime
	// =========================================================================
	FTimerHandle MatchTimerHandle;

	FText ServerAnnouncePrefixText;
	bool bAnnouncementExternallyDriven = false;
};
