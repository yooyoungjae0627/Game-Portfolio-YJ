#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/MosesGameModeBase.h"
#include "UE5_Multi_Shooter/Match/MosesMatchPhase.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;
class UMSCharacterCatalog;
class UMosesExperienceManagerComponent;
class UMosesExperienceDefinition;
class AMosesMatchGameState;
class AMosesPlayerState;

/**
 * AMosesMatchGameMode
 *
 * 역할:
 * - 서버 권위 Phase 머신(WaitingForPlayers → Warmup → Combat → Result)
 * - Phase에 맞춰 Experience 전환
 * - MatchGameState에 Phase/RemainingSeconds/Announcement 확정
 * - DAY11: Respawn 스케줄 + Result 승패 판정 확정(Combat→Result 진입 시)
 *
 * 정책:
 * - Server Authority 100%
 * - GameMode=결정(Phase/Experience/ServerTravel/Respawn/Result)
 * - GameState=복제(HUD 갱신용 데이터)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesMatchGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesMatchGameMode();

	/** 디버그용: 서버에서 즉시 로비로 */
	UFUNCTION(Exec)
	void TravelToLobby();

	UPROPERTY(EditDefaultsOnly, Category = "Match|Debug")
	float AutoReturnToLobbySeconds = 0.0f;

protected:
	// -------------------------------------------------------------------------
	// Engine
	// -------------------------------------------------------------------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
	// -------------------------------------------------------------------------
	// Experience READY Hook
	// -------------------------------------------------------------------------
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience) override;

private:
	// -------------------------------------------------------------------------
	// Match Phase (Server Authoritative)
	// -------------------------------------------------------------------------
	void StartMatchFlow_AfterExperienceReady();
	void SetMatchPhase(EMosesMatchPhase NewPhase);
	void AdvancePhase();
	void HandlePhaseTimerExpired();

	float GetPhaseDurationSeconds(EMosesMatchPhase Phase) const;

private:
	// -------------------------------------------------------------------------
	// Phase -> Experience Switch
	// -------------------------------------------------------------------------
	static FName GetExperienceNameForPhase(EMosesMatchPhase Phase);
	void ServerSwitchExperienceByPhase(EMosesMatchPhase Phase);
	UMosesExperienceManagerComponent* GetExperienceManager() const;

private:
	// -------------------------------------------------------------------------
	// MatchGameState access
	// -------------------------------------------------------------------------
	AMosesMatchGameState* GetMatchGameState() const;

private:
	// -------------------------------------------------------------------------
	// Match default loadout (Server only)
	// -------------------------------------------------------------------------
	void Server_EnsureDefaultMatchLoadout(APlayerController* PC, const TCHAR* FromWhere);

private:
	// -------------------------------------------------------------------------
	// DAY11 [MOD] Respawn schedule (Server only)
	// - PlayerState(GAS Death 확정) -> GameMode가 RestartPlayer를 확정한다.
	// -------------------------------------------------------------------------
public:
	void ServerScheduleRespawn(AController* Controller, float DelaySeconds);

private:
	void HandleRespawnTimerExpired(AController* Controller);

	// -------------------------------------------------------------------------
	// DAY11 [MOD] Result decide (Server only)
	// - Combat -> Result 진입 시 서버가 승패 확정
	// - Captures -> PvPKills -> ZombieKills -> Headshots -> Draw
	// -------------------------------------------------------------------------
	void ServerDecideResult_OnEnterResultPhase();

	bool TryChooseWinnerByReason_Server(
		const TArray<AMosesPlayerState*>& Players,
		FString& OutWinnerId,
		EMosesResultReason& OutReason) const;

	// -------------------------------------------------------------------------
	// Travel / Debug
	// -------------------------------------------------------------------------
	FString GetLobbyMapURL() const;
	void HandleAutoReturn();

	void DumpPlayerStates(const TCHAR* Prefix) const;
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	bool CanDoServerTravel() const;

	// -------------------------------------------------------------------------
	// PlayerStart helpers
	// -------------------------------------------------------------------------
	void CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const;
	void FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const;
	void ReserveStartForController(AController* Player, APlayerStart* Start);
	void ReleaseReservedStart(AController* Player);
	void DumpReservedStarts(const TCHAR* Where) const;

	// -------------------------------------------------------------------------
	// PawnClass Resolve
	// -------------------------------------------------------------------------
	UClass* ResolvePawnClassFromSelectedId(int32 SelectedId) const;

	// -------------------------------------------------------------------------
	// Experience Ready Poll (Tick 금지)
	// -------------------------------------------------------------------------
	void StartWarmup_WhenExperienceReady();
	void PollExperienceReady_AndStartWarmup();

	void ServerSaveRecord_Once_OnEnterResult();

private:
	FTimerHandle ExperienceReadyPollHandle;
	int32 ExperienceReadyPollCount = 0;

	static constexpr float ExperienceReadyPollInterval = 0.2f;
	static constexpr int32  ExperienceReadyPollMaxCount = 50; // 10초

	// -------------------------------------------------------------------------
	// Phase config
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float WarmupSeconds = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float CombatSeconds = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float ResultSeconds = 30.0f;

	// -------------------------------------------------------------------------
	// Phase runtime (Server only)
	// -------------------------------------------------------------------------
	EMosesMatchPhase CurrentPhase = EMosesMatchPhase::WaitingForPlayers;

	FTimerHandle PhaseTimerHandle;
	double PhaseEndTimeSeconds = 0.0;

	// -------------------------------------------------------------------------
	// Misc timers
	// -------------------------------------------------------------------------
	FTimerHandle AutoReturnTimerHandle;

	// -------------------------------------------------------------------------
	// PlayerStart reservation
	// -------------------------------------------------------------------------
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerStart>> ReservedPlayerStarts;

	UPROPERTY()
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStartByController;

	// -------------------------------------------------------------------------
	// Assets
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Match|Pawn")
	TObjectPtr<UMosesPawnData> MatchPawnData = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TSubclassOf<APawn> FallbackPawnClass;

	bool bRecordSavedThisMatch = false;
};
