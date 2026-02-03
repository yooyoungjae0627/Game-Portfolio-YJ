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
	bool bPopupVisible = false;
};
