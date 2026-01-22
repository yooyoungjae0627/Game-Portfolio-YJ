#pragma once

#include "UE5_Multi_Shooter/GameMode/GameMode/MosesGameModeBase.h"
#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;
class UMSCharacterCatalog;
class UMosesExperienceManagerComponent;
class UMosesExperienceDefinition;

UENUM(BlueprintType)
enum class EMosesMatchPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "WaitingForPlayers"),
	Warmup            UMETA(DisplayName = "Warmup"),
	Combat            UMETA(DisplayName = "Combat"),
	Result            UMETA(DisplayName = "Result"),
};

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesMatchGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesMatchGameMode();

	UFUNCTION(Exec)
	void TravelToLobby();

	UPROPERTY(EditDefaultsOnly, Category = "Match|Debug")
	float AutoReturnToLobbySeconds = 0.0f;

protected:
	// ------------------------------------------------------------
	// Engine
	// ------------------------------------------------------------
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
	// ------------------------------------------------------------
	// Experience READY Hook
	// ------------------------------------------------------------
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience) override;

private:
	// ------------------------------------------------------------
	// Match Phase (Server Authoritative)
	// ------------------------------------------------------------
	void StartMatchFlow_AfterExperienceReady();
	void SetMatchPhase(EMosesMatchPhase NewPhase);
	void AdvancePhase();
	void HandlePhaseTimerExpired();

	float GetPhaseDurationSeconds(EMosesMatchPhase Phase) const;

	// ------------------------------------------------------------
	// Phase -> Experience Switch
	// ------------------------------------------------------------
	static FName GetExperienceNameForPhase(EMosesMatchPhase Phase);
	void ServerSwitchExperienceByPhase(EMosesMatchPhase Phase);
	UMosesExperienceManagerComponent* GetExperienceManager() const;

private:
	// ------------------------------------------------------------
	// Travel / Debug
	// ------------------------------------------------------------
	FString GetLobbyMapURL() const;
	void HandleAutoReturn();

	void DumpPlayerStates(const TCHAR* Prefix) const;
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	bool CanDoServerTravel() const;

private:
	// ------------------------------------------------------------
	// PlayerStart helpers
	// ------------------------------------------------------------
	void CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const;
	void FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const;
	void ReserveStartForController(AController* Player, APlayerStart* Start);
	void ReleaseReservedStart(AController* Player);
	void DumpReservedStarts(const TCHAR* Where) const;

private:
	// ------------------------------------------------------------
	// PawnClass Resolve
	// ------------------------------------------------------------
	UClass* ResolvePawnClassFromSelectedId(int32 SelectedId) const;

private:
	// ------------------------------------------------------------
	// Phase config
	// ------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float WarmupSeconds = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float CombatSeconds = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float ResultSeconds = 30.0f;

private:
	// ------------------------------------------------------------
	// Phase runtime state (Server only)
	// ------------------------------------------------------------
	EMosesMatchPhase CurrentPhase = EMosesMatchPhase::WaitingForPlayers;

	FTimerHandle PhaseTimerHandle;
	double PhaseEndTimeSeconds = 0.0;

private:
	// ------------------------------------------------------------
	// Misc timers
	// ------------------------------------------------------------
	FTimerHandle AutoReturnTimerHandle;

private:
	// ------------------------------------------------------------
	// PlayerStart reservation
	// ------------------------------------------------------------
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerStart>> ReservedPlayerStarts;

	UPROPERTY()
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStartByController;

private:
	// ------------------------------------------------------------
	// Assets
	// ------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Match|Pawn")
	TObjectPtr<UMosesPawnData> MatchPawnData = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TSubclassOf<APawn> FallbackPawnClass;
};
