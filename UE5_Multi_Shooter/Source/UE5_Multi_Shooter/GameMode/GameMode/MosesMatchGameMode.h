#pragma once

#include "MosesGameModeBase.h"
#include "MosesMatchGameMode.generated.h"

class APlayerStart;
class AController;
class APlayerController;
class UMosesPawnData;

/**
 * Match 전용 GameMode
 * - 매치 규칙 + 로비 복귀 Travel + 디버그 로그
 * - PlayerStart 중복 방지(예약/해제)
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

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

private:
	/** 매치에서 PC에게 Pawn을 스폰하고 빙의시키는 단일 함수(정책 고정) */
	void SpawnAndPossessMatchPawn(APlayerController* PC);

	/** 이미 유효한 Pawn이 있으면 재사용할지/폐기할지 정책 */
	bool ShouldRespawnPawn(APlayerController* PC) const;


private:
	/*====================================================
	= Internal helpers
	====================================================*/
	FString GetLobbyMapURL() const;
	void DumpPlayerStates(const TCHAR* Prefix) const;

	/** ✅ DoD 고정 포맷 덤프(AMosesPlayerState만) */
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	/** 서버에서만 Travel하도록 방어 */
	bool CanDoServerTravel() const;

	/** Match에서 사용할 PlayerStart 목록 수집(Tag=Match) */
	void CollectMatchPlayerStarts(TArray<APlayerStart*>& OutStarts) const;

	/** 이미 점유된 Start를 제외한 후보만 만들기 */
	void FilterFreeStarts(const TArray<APlayerStart*>& InAll, TArray<APlayerStart*>& OutFree) const;

	/** PC에 PlayerStart를 예약(중복 방지) */
	void ReserveStartForController(AController* Player, APlayerStart* Start);

	/** PC에 예약된 PlayerStart 해제 */
	void ReleaseReservedStart(AController* Player);

	/** 디버그: 현재 예약 상태 덤프 */
	void DumpReservedStarts(const TCHAR* Where) const;

private:
	/*====================================================
	= Timers
	====================================================*/
	FTimerHandle AutoReturnTimerHandle;
	void HandleAutoReturn();

private:
	/*====================================================
	= PlayerStart reservation (duplicate prevention)
	====================================================*/
	// 중복 방지: 이미 할당된 PlayerStart 추적
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerStart>> ReservedPlayerStarts;

	// PC별 할당된 Start (해제용)
	UPROPERTY()
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStartByController;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Pawn")
	TObjectPtr<UMosesPawnData> MatchPawnData;
};
