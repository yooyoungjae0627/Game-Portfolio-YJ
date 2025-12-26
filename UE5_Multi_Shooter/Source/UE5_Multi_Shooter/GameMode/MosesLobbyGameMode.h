#pragma once

#include "MosesGameModeBase.h"
#include "MosesLobbyGameMode.generated.h"

class AMosesPlayerController;

/**
 * Lobby GameMode
 * - 로비 전용 규칙 + Match로 ServerTravel 트리거 담당
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameMode();

	// PC Server RPC가 도착한 뒤, 서버가 최종 처리하는 루트
	void HandleStartGameRequest(AMosesPlayerController* RequestPC);

	/** 서버 콘솔에서 매치로 이동 */
	UFUNCTION(Exec)
	void TravelToMatch();

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	/** 복귀 후 정책 적용/증명 로그용 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Travel 직전/후 PS 유지 증명용 로그 포인트 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

private:
	/** Match 맵 URL 정책을 한 곳에서 고정 */
	FString GetMatchMapURL() const;

	/** PS 유지/재생성 검증을 위한 PlayerArray 덤프(기존) */
	void DumpPlayerStates(const TCHAR* Prefix) const;

	/** DoD 고정 포맷 덤프(AMosesPlayerState만) */
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	/** 서버에서만 ServerTravel하도록 방어 */
	bool CanDoServerTravel() const;

	/** 로비 복귀 정책: Ready 초기화(추천) */
	void ApplyReturnPolicy(APlayerController* PC);

	int32 GetPlayerCount() const;
	int32 GetReadyCount() const;
};
