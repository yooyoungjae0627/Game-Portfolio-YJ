// ============================================================================
// UE5_Multi_Shooter/Match/GameMode/MosesMatchGameMode.h  (CLEANED)
// - Server authoritative match flow (Waiting -> Warmup -> Combat -> Result)
// - Phase -> Experience switch
// - Respawn + Result decide + Auto return
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/MosesGameModeBase.h"
#include "UE5_Multi_Shooter/Match/MosesMatchPhase.h"

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

enum class EMosesResultReason : uint8;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesMatchGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesMatchGameMode();

	// Debug: server 즉시 로비
	UFUNCTION(Exec) void TravelToLobby();

	// Result Confirm -> Return Lobby (Server only)
	bool Server_RequestReturnToLobbyFromPlayer(APlayerController* Requestor);

	UPROPERTY(EditDefaultsOnly, Category = "Match|Debug")
	float AutoReturnToLobbySeconds = 0.0f;

protected:
	// =========================================================================
	// Engine
	// =========================================================================
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
	// =========================================================================
	// Experience READY hook
	// =========================================================================
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience) override;

private:
	// =========================================================================
	// Match phase machine (Server only)
	// =========================================================================
	void StartMatchFlow_AfterExperienceReady();
	void SetMatchPhase(EMosesMatchPhase NewPhase);
	void AdvancePhase();
	void HandlePhaseTimerExpired();
	float GetPhaseDurationSeconds(EMosesMatchPhase Phase) const;

private:
	// =========================================================================
	// Phase -> Experience switch
	// =========================================================================
	static FName GetExperienceNameForPhase(EMosesMatchPhase Phase);
	void ServerSwitchExperienceByPhase(EMosesMatchPhase Phase);
	UMosesExperienceManagerComponent* GetExperienceManager() const;

private:
	// =========================================================================
	// GameState access
	// =========================================================================
	AMosesMatchGameState* GetMatchGameState() const;

private:
	// =========================================================================
	// Default loadout (Server only)
	// =========================================================================
	void Server_EnsureDefaultMatchLoadout(APlayerController* PC, const TCHAR* FromWhere);

public:
	// =========================================================================
	// Respawn schedule (Server only)
	// =========================================================================
	void ServerScheduleRespawn(AController* Controller, float DelaySeconds);

private:
	void ServerExecuteRespawn(AController* Controller);

	UPROPERTY()
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimerHandlesByController;

private:
	// =========================================================================
	// Result decide (Server only)
	// =========================================================================
	void ServerDecideResult_OnEnterResultPhase();

	bool TryChooseWinnerByReason_Server(
		const TArray<AMosesPlayerState*>& Players,
		FString& OutWinnerId,
		EMosesResultReason& OutReason) const;

private:
	// =========================================================================
	// Travel / Debug
	// =========================================================================
	FString GetLobbyMapURL() const;
	void HandleAutoReturn();

	void DumpPlayerStates(const TCHAR* Prefix) const;
	void DumpAllDODPlayerStates(const TCHAR* Where) const;
	bool CanDoServerTravel() const;

private:
	// =========================================================================
	// PlayerStart reservation
	// =========================================================================
	void CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const;
	void FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const;
	void ReserveStartForController(AController* Player, APlayerStart* Start);
	void ReleaseReservedStart(AController* Player);
	void DumpReservedStarts(const TCHAR* Where) const;

private:
	// =========================================================================
	// PawnClass resolve
	// =========================================================================
	UClass* ResolvePawnClassFromSelectedId(int32 SelectedId) const;

private:
	// =========================================================================
	// Experience Ready Poll (Tick 금지)
	// =========================================================================
	void StartWarmup_WhenExperienceReady();
	void PollExperienceReady_AndStartWarmup();

	void ServerSaveRecord_Once_OnEnterResult();

private:
	// =========================================================================
	// Config: phase durations
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase") float WarmupSeconds = 20.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase") float CombatSeconds = 60.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase") float ResultSeconds = 30.0f;

private:
	// =========================================================================
	// Runtime: phase
	// =========================================================================
	EMosesMatchPhase CurrentPhase = EMosesMatchPhase::WaitingForPlayers;

	FTimerHandle PhaseTimerHandle;
	double PhaseEndTimeSeconds = 0.0;

private:
	// =========================================================================
	// Runtime: misc timers
	// =========================================================================
	FTimerHandle ExperienceReadyPollHandle;
	int32 ExperienceReadyPollCount = 0;

	static constexpr float ExperienceReadyPollInterval = 0.2f;
	static constexpr int32  ExperienceReadyPollMaxCount = 50;

	FTimerHandle AutoReturnTimerHandle;

private:
	// =========================================================================
	// PlayerStart reservation maps
	// =========================================================================
	UPROPERTY() TSet<TWeakObjectPtr<APlayerStart>> ReservedPlayerStarts;
	UPROPERTY() TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStartByController;

private:
	// =========================================================================
	// Assets
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Match|Pawn")
	TObjectPtr<UMosesPawnData> MatchPawnData = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TSubclassOf<APawn> FallbackPawnClass;

private:
	// =========================================================================
	// Persist / Result guards
	// =========================================================================
	bool bRecordSavedThisMatch = false;
	bool bResultComputedThisMatch = false;
};
