#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesMatchRulePopupWidget.generated.h"

class UButton;
class UOverlay;
class UTextBlock;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesMatchRulePopupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

public:
	void SetPopupVisible(bool bVisible);

private:
	UFUNCTION()
	void HandleToggleClicked();

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ToggleButton = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UOverlay> PopupRoot = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RulesText = nullptr;

private:
	bool bPopupVisible = false;
};
