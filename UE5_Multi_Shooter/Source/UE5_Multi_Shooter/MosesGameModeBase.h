// ============================================================================
// UE5_Multi_Shooter/MosesGameModeBase.h  (CLEANED)
// - Server-only control tower
// - Experience select/load + SpawnGate (READY 전 스폰 금지)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"

#include "MosesGameModeBase.generated.h"

class UMosesExperienceDefinition;
class UMosesPawnData;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

protected:
	AMosesGameModeBase();

	// =========================================================================
	// Engine: Experience bootstrap
	// =========================================================================
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() final;

	// =========================================================================
	// Engine: login/spawn hooks (SpawnGate)
	// =========================================================================
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// READY 전: 큐잉하고 return / READY 후: 정상 스폰(Flush에서 Super 호출)
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) final;

	// PawnData 주입 -> FinishSpawning (Defer Spawn)
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) final;

	// 최종 스폰 지점: READY 전이면 스폰 안 함 (정책 강제)
	virtual void RestartPlayer(AController* NewPlayer) override;

	// 디버그/추적
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

public:
	// =========================================================================
	// Travel helpers (server)
	// =========================================================================
	virtual void ServerTravelToLobby();

	// =========================================================================
	// PawnData resolve
	// =========================================================================
	const UMosesPawnData* GetPawnDataForController(const AController* InController) const;

	// 디버그용
	FString GetConnAddr(APlayerController* PC);

protected:
	// =========================================================================
	// DoD hook: Experience READY 이후 파생 GM이 phase/room 등을 확정
	// =========================================================================
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience);

private:
	// =========================================================================
	// Experience assignment flow
	// =========================================================================
	void HandleMatchAssignmentIfNotExpectingOne();        // NextTick
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId);

	bool IsExperienceLoaded() const;

	void OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience); // READY 콜백
	void OnExperienceReady_SpawnGateRelease();
	void FlushPendingPlayers();

private:
	// =========================================================================
	// Pending players (READY 전 스폰 대기)
	// =========================================================================
	UPROPERTY()
	TArray<TWeakObjectPtr<APlayerController>> PendingStartPlayers;

	bool bFlushingPendingPlayers = false;
	bool bDoD_ExperienceSelectedLogged = false;
};
