#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFeatureAction_MosesCombatUI.generated.h"

class UUserWidget;
class UWorld;
class ULocalPlayer;

/**
 * UGameFeatureAction_MosesCombatUI
 *
 * 목표:
 * - 매치 월드에서만 HUD를 주입한다(Lobby/Start에서는 붙이면 안 됨).
 * - SeamlessTravel/PIE에서 PC 생성 타이밍 문제로 HUD가 누락되는 경우를 재시도로 보정한다.
 * - LocalPlayer별 HUD 1개를 보장한다.
 * - 월드 종료 시 위젯/타이머를 완전 정리한다.
 */
UCLASS()
class GF_COMBAT_UIRUNTIME_API UGameFeatureAction_MosesCombatUI : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	UGameFeatureAction_MosesCombatUI();

protected:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	struct FPerWorldData
	{
		TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<UUserWidget>> WidgetByLocalPlayer;
		int32 RetryCount = 0;
		FTimerHandle RetryTimerHandle;
	};

private:
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	bool ShouldInjectForWorld(const UWorld* World) const;

	void InjectForWorld(UWorld* World);
	void RemoveForWorld(UWorld* World);

	void ScheduleRetryInject(UWorld* World);
	void RetryInject(UWorld* World);

private:
	bool bIsActive = false;

	TMap<TWeakObjectPtr<UWorld>, FPerWorldData> WorldDataByWorld;

	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle WorldCleanupHandle;

private:
	/** 매치 HUD 위젯 클래스 (WBP도 OK) */
	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	int32 ViewportZOrder = 10;

	/** MatchLevel 필터 토큰(PIE prefix 대응: Contains 검사) */
	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	FString MatchWorldNameToken = TEXT("MatchLevel");

	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	int32 MaxRetryCount = 20;

	UPROPERTY(EditAnywhere, Category = "Moses|UI")
	float RetryIntervalSeconds = 0.2f;
};
