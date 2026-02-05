#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesEventDrivenWidgetBase.generated.h"

/**
 * Event-driven HUD Base
 * - Tick 금지(헌법): Tick이 호출되면 경고 로그로 바로 드러나게 만든다.
 * - HUD는 Delegate 수신 시에만 갱신
 */
UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesEventDrivenWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesEventDrivenWidgetBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// ✅ [FIX] Tick 들어오면 경고로 “헌법 위반”을 증거화
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override final;

	/** 파생 위젯에서 Delegate 바인딩 */
	virtual void BindDelegates() {}

	/** 파생 위젯에서 Delegate 해제 */
	virtual void UnbindDelegates() {}

private:
	bool bDelegatesBound = false;
};
