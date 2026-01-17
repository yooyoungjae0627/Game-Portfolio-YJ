#pragma once

#include "MosesGameModeBase.h"
#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;
class UMSCharacterCatalog;

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
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesMatchGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesMatchGameMode();

	/** 서버 콘솔에서 로비로 복귀 (Day4 테스트용) */
	UFUNCTION(Exec)
	void TravelToLobby();

	/** (선택) 매치 시작 후 N초 뒤 자동 복귀 테스트 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Debug")
	float AutoReturnToLobbySeconds = 0.0f;

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	/** 매치 진입/유지 검증 로그(PC/PS 유지 확인) */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 플레이어 나갈 때 PlayerStart 점유 해제 */
	virtual void Logout(AController* Exiting) override;

	/** 랜덤 PlayerStart 선택(중복 방지) */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Seamless Travel 인계 과정 로그 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

	/** [DAY2-MOD] SpawnDefaultPawnFor는 "선택된 PawnClass"로 Spawn 되도록 보장한다. */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** 컨트롤러별 기본 PawnClass 결정(SelectedId -> Catalog) */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

private:
	void SpawnAndPossessMatchPawn(APlayerController* PC);
	bool ShouldRespawnPawn(APlayerController* PC) const;

private:
	FString GetLobbyMapURL() const;
	void DumpPlayerStates(const TCHAR* Prefix) const;
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	bool CanDoServerTravel() const;

	void CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const;
	void FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const;
	void ReserveStartForController(AController* Player, APlayerStart* Start);
	void ReleaseReservedStart(AController* Player);
	void DumpReservedStarts(const TCHAR* Where) const;

private:
	FTimerHandle AutoReturnTimerHandle;
	void HandleAutoReturn();

private:
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerStart>> ReservedPlayerStarts;

	UPROPERTY()
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStartByController;

	/**
	 * MatchPawnData
	 * - [주의] PawnClass 선택은 SelectedId/Catalog가 "단일 진실"이다.
	 * - PawnData는 Pawn 자체(각 캐릭터 BP)의 PawnExtensionComponent에서 들고 가는 구조가 이미 존재하므로,
	 *   여기 GameMode의 PawnData는 Day2 스폰 결정에는 쓰지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Pawn")
	TObjectPtr<UMosesPawnData> MatchPawnData = nullptr; // (유지: 필요시 확장)

private:
	UClass* ResolvePawnClassFromSelectedId(int32 SelectedId) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|CharacterSelect")
	TSubclassOf<APawn> FallbackPawnClass;
};
