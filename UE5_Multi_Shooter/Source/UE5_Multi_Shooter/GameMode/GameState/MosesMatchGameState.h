// ============================================================================
// MosesMatchGameState.h (FULL)
// - Match 전용 GameState (Phase / RemainingSeconds / Announcement)
// - 부모: AMosesGameState (ExperienceManagerComponent 공통 보장)
// - 정책:
//   - 서버만 변경(Server Authority 100%)
//   - 클라는 RepNotify -> Delegate로 HUD 갱신 (Tick/Binding 금지)
// - [MOD] PushModel 대응: 서버에서 값 변경 시 Dirty 마킹/ForceNetUpdate를 사용한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesGameState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchTypes.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h"

#include "MosesMatchGameState.generated.h"

// HUD 갱신: RepNotify -> Delegate
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesMatchTimeChangedNative, int32 /*RemainingSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesMatchPhaseChangedNative, EMosesMatchPhase /*Phase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesAnnouncementChangedNative, const FMosesAnnouncementState& /*State*/);

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

public:
	// -------------------------------------------------------------------------
	// Getters
	// -------------------------------------------------------------------------
	int32 GetRemainingSeconds() const { return RemainingSeconds; }
	EMosesMatchPhase GetMatchPhase() const { return MatchPhase; }
	const FMosesAnnouncementState& GetAnnouncementState() const { return AnnouncementState; }

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

private:
	// -------------------------------------------------------------------------
	// Server-only
	// -------------------------------------------------------------------------
	FTimerHandle MatchTimerHandle;
	FText ServerAnnouncePrefixText;
};
