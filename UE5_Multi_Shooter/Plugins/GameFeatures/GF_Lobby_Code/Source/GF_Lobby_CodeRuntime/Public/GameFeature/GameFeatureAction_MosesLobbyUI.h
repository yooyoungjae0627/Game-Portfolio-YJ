#pragma once

#include "GameFeatureAction.h"
#include "GameFeatureAction_MosesLobbyUI.generated.h"

/**
 * 공부 메모)
 * - GameFeatureAction은 "GF가 켜질 때/꺼질 때" 엔진이 호출해주는 훅 포인트다.
 * - 여기서 UI를 직접 만들 수도 있지만,
 *   LocalPlayerSubsystem에 위임하면 역할이 깔끔해진다.
 *   (GF: 트리거 / Subsystem: 실제 UI&입력 처리)
 */
UCLASS()
class UGameFeatureAction_MosesLobbyUI : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	void ForEachLocalPlayer_Do(UWorld* World, TFunctionRef<void(class ULocalPlayer*)> Fn);
};
