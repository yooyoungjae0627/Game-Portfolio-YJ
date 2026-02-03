#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesScopeWidget.generated.h"

/**
 * UMosesScopeWidget
 *
 * [역할]
 * - 스나 스코프 오버레이(원형 마스크/라인) 표시 토글.
 *
 * [규칙]
 * - Tick 금지
 * - Designer Binding 금지
 * - PlayerController(로컬)에서 Scope On/Off 이벤트로만 토글한다.
 */
UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesScopeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesScopeWidget(const FObjectInitializer& ObjectInitializer);

public:
	void SetScopeVisible(bool bVisible);

protected:
	virtual void NativeOnInitialized() override;
};
