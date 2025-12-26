#pragma once

#include "CoreMinimal.h"
#include "MosesGameModeBase.h"
#include "MosesMatchGameMode.generated.h"

/**
 * Match 전용 GameMode
 * - 매치 규칙 + 로비 복귀 Travel + 디버그 로그
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

	/** Seamless Travel 인계 과정 로그 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

private:
	FString GetLobbyMapURL() const;
	void DumpPlayerStates(const TCHAR* Prefix) const;

	/** ✅ DoD 고정 포맷 덤프(AMosesPlayerState만) */
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	/** 서버에서만 Travel하도록 방어 */
	bool CanDoServerTravel() const;

	FTimerHandle AutoReturnTimerHandle;
	void HandleAutoReturn();
};
