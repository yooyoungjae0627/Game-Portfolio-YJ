// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MosesGameModeBase.h"
#include "MosesLobbyGameMode.generated.h"

/**
 * ALobbyGameMode
 * - 로비 전용 서버 규칙
 * - Ready/Start(추후 확장) + Match로 ServerTravel 트리거 담당
 *
 * 설계 원칙:
 * - Experience 로딩/스폰 게이트는 AMosesGameModeBase가 담당한다.
 * - 로비 GM은 "로비 규칙"과 "Travel"만 담당한다.
 */

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()
	
public:
	AMosesLobbyGameMode();

	/** 서버 콘솔에서 매치로 이동 */
	UFUNCTION(Exec)
	void TravelToMatch();

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	/** Travel 직전/후 PS 유지 증명용 로그 포인트 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

private:
	/** Match 맵 URL 정책을 한 곳에서 고정 */
	FString GetMatchMapURL() const;

	/** PS 유지/재생성 검증을 위한 PlayerArray 덤프 */
	void DumpPlayerStates(const TCHAR* Prefix) const;

	/** 서버에서만 ServerTravel하도록 방어 */
	bool CanDoServerTravel() const;
	
};
