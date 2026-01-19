#pragma once

#include "MosesGameModeBase.h"
#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;
class UMSCharacterCatalog;

/**
 * [ADD] 매치 진행 페이즈(서버 권위로만 결정)
 * - GameMode는 서버에만 존재하므로, 이 값 자체는 "서버 로직"에 쓰고
 * - 클라 UI/HUD 표시용은 GameState에 Replicated 값으로 따로 내려주는 게 정석이다.
 *
 * (이번 파일에서는 "서버 진행 확정"을 먼저 고정하고,
 *  GameState 동기화는 추후 확장 포인트로 주석 처리한다.)
 */
UENUM(BlueprintType)
enum class EMosesMatchPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "WaitingForPlayers"),
	Warmup            UMETA(DisplayName = "Warmup"),
	Combat            UMETA(DisplayName = "Combat"),
	Result            UMETA(DisplayName = "Result"),
};

/**
 * AMosesMatchGameMode
 *
 * [목표]
 * - Lobby에서 서버가 확정한 PlayerState.SelectedCharacterId를 기반으로
 *   MatchLevel에서 "선택된 캐릭터 PawnClass"를 Spawn/Possess 한다.
 *
 * [정책]
 * - 서버 권위: PawnClass 선택도 서버에서 결정한다.
 * - 클라는 결과(스폰된 Pawn 및 Possess)를 replication으로 받는다.
 *
 * [MOD] 추가 목표(기획 반영)
 * - Experience READY 이후에만 매치 진행(타이머)을 시작한다.
 * - Warmup(2m) → Combat(10m) → Result(30s) → Lobby 복귀
 */
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
