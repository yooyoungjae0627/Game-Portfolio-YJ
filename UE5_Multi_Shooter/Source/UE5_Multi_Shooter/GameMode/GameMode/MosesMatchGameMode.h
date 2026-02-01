#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesGameModeBase.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchPhase.h" // [MOD] enum 분리(순환 방지)

#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;
class UMSCharacterCatalog;
class UMosesExperienceManagerComponent;
class UMosesExperienceDefinition;
class AMosesMatchGameState;

/**
 * AMosesMatchGameMode
 *
 * 역할:
 * - 서버 권위 Phase 머신(WaitingForPlayers → Warmup → Combat → Result)
 * - Phase에 맞춰 Experience 전환(네 구조 유지)
 * - MatchGameState에 Phase/RemainingSeconds/Announcement를 확정한다.
 *
 * 정책:
 * - Server Authority 100%
 * - GameMode=결정(Phase/Experience/ServerTravel)
 * - GameState=복제(클라 HUD 갱신용 데이터)
 * - RemainingSeconds는 "MatchGameState의 서버 타이머"가 단일 진실로 관리한다.
 */
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
	// [MOD] Match default loadout (Server only)
	// - MatchLevel 진입 시 "기본 Rifle + 30/90"를 SSOT(PlayerState)에서 보장한다.
	// - PostLogin / SeamlessTravel / READY(Restart) 경로 모두 커버한다.
	// -------------------------------------------------------------------------
	void Server_EnsureDefaultMatchLoadout(APlayerController* PC, const TCHAR* FromWhere); // ✅ [MOD]

private:
	// -------------------------------------------------------------------------
	// Travel / Debug
	// -------------------------------------------------------------------------
	FString GetLobbyMapURL() const;
	void HandleAutoReturn();

	void DumpPlayerStates(const TCHAR* Prefix) const;
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	bool CanDoServerTravel() const;

private:
	// -------------------------------------------------------------------------
	// PlayerStart helpers
	// -------------------------------------------------------------------------
	void CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const;
	void FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const;
	void ReserveStartForController(AController* Player, APlayerStart* Start);
	void ReleaseReservedStart(AController* Player);
	void DumpReservedStarts(const TCHAR* Where) const;

private:
	// -------------------------------------------------------------------------
	// PawnClass Resolve
	// -------------------------------------------------------------------------
	UClass* ResolvePawnClassFromSelectedId(int32 SelectedId) const;

	// [MOD] Experience Ready 폴링(틱 금지)로 Warmup 시작 보장
private:
	void StartWarmup_WhenExperienceReady();
	void PollExperienceReady_AndStartWarmup();

private:
	FTimerHandle ExperienceReadyPollHandle;
	int32 ExperienceReadyPollCount = 0;

	static constexpr float ExperienceReadyPollInterval = 0.2f;
	static constexpr int32  ExperienceReadyPollMaxCount = 50; // 10초

private:
	// -------------------------------------------------------------------------
	// Phase config
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float WarmupSeconds = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float CombatSeconds = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float ResultSeconds = 30.0f;

private:
	// -------------------------------------------------------------------------
	// Phase runtime (Server only)
	// -------------------------------------------------------------------------
	EMosesMatchPhase CurrentPhase = EMosesMatchPhase::WaitingForPlayers;

	FTimerHandle PhaseTimerHandle;
	double PhaseEndTimeSeconds = 0.0;

private:
	// -------------------------------------------------------------------------
	// Misc timers
	// -------------------------------------------------------------------------
	FTimerHandle AutoReturnTimerHandle;

private:
	// -------------------------------------------------------------------------
	// PlayerStart reservation
	// -------------------------------------------------------------------------
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerStart>> ReservedPlayerStarts;

	UPROPERTY()
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStartByController;

private:
	// -------------------------------------------------------------------------
	// Assets
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Match|Pawn")
	TObjectPtr<UMosesPawnData> MatchPawnData = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TSubclassOf<APawn> FallbackPawnClass;
};
