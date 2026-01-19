#pragma once

#include "MosesGameModeBase.h"
#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;
class UMSCharacterCatalog;
class UMosesExperienceManagerComponent;

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

	/** 서버 콘솔에서 로비로 복귀 (디버깅/테스트용) */
	UFUNCTION(Exec)
	void TravelToLobby();

	/** (선택) 매치 시작 후 N초 뒤 자동 복귀 테스트 */
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

	/** SpawnDefaultPawnFor는 "선택된 PawnClass"로 Spawn 되도록 보장한다. */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** 컨트롤러별 기본 PawnClass 결정(SelectedId -> Catalog) */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
	/**
	 * [ADD] Experience READY 이후 훅
	 * - Base(GameModeBase)가 [EXP][READY]가 되는 순간 호출해준다.
	 * - 여기서부터는 "스폰/Ability/Input/Feature 적용"이 안전하므로
	 *   Match 페이즈 타이머를 서버에서 확정해서 시작한다.
	 */
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
	// [ADD] Phase -> Experience Switch
	// ------------------------------------------------------------
	/** [ADD] Phase에 해당하는 Experience 이름(PrimaryAssetName) 반환 */
	FName GetExperienceNameForPhase(EMosesMatchPhase Phase);

	/**
	 * [ADD] 서버 권위로 Experience를 교체한다.
	 * - Warmup  -> Exp_Match_Warmup
	 * - Combat  -> Exp_Match_Combat
	 * - Result  -> Exp_Match_Result
	 *
	 * 주의:
	 * - ExperienceManagerComponent는 GameState에 붙어 있음
	 * - 서버에서 호출하면 CurrentExperienceId가 복제되어 클라도 따라온다.
	 */
	void ServerSwitchExperienceByPhase(EMosesMatchPhase Phase);

	/** [ADD] ExperienceManagerComponent 접근 헬퍼(없으면 nullptr) */
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

	/** [ADD] 2분 파밍/준비 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float WarmupSeconds = 120.0f;

	/** [ADD] 10분 전투 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float CombatSeconds = 600.0f;

	/** [ADD] 30초 결과 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phase")
	float ResultSeconds = 30.0f;

private:
	// ------------------------------------------------------------
	// Phase runtime state (Server only)
	// ------------------------------------------------------------

	/** [ADD] 현재 페이즈 */
	EMosesMatchPhase CurrentPhase = EMosesMatchPhase::WaitingForPlayers;

	/** [ADD] 페이즈 타이머 핸들 */
	FTimerHandle PhaseTimerHandle;

	/** [ADD] (옵션) 페이즈 종료 월드 시간(서버 기준) */
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
	TObjectPtr<UMosesPawnData> MatchPawnData = nullptr; // (유지: 필요시 확장)

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TSubclassOf<APawn> FallbackPawnClass;
};
