// ============================================================================
// MosesMatchRulePopupWidget.h
// - 룰 팝업(열기/닫기) 위젯
// - PopupRoot(딤+본문 포함)의 Visibility를 토글한다.
// - [MOD] UI Only/Game Only 입력 모드 토글을 "위젯 내부"에서 최소 구현.
//         (프로젝트에 중앙 Input 정책이 있다면 PC/HUD로 이관 가능)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesMatchRulePopupWidget.generated.h"

class UButton;
class UOverlay;
class UTextBlock;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchRulePopupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleToggleClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void SetPopupVisible(bool bVisible);
	void ApplyInputModeForPopup(bool bPopupVisible);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ToggleButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton = nullptr;

	/** 팝업 루트(딤+본문을 포함하는 최상위 Overlay/Border 등) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> PopupRoot = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RulesText = nullptr;

private:
	bool bPopupVisible = false;
};
