// ============================================================================
// MosesPickupPromptWidget.h (FULL)
// ----------------------------------------------------------------------------
// - 월드 말풍선 프롬프트 위젯 C++ 래퍼
// - Tick/Binding 금지: 외부에서 SetPromptTexts로만 갱신
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MosesPickupPromptWidget.generated.h"

class UTextBlock;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesPickupPromptWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

public:
	void SetPromptTexts(const FText& InInteractText, const FText& InItemNameText);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_Interact = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_ItemName = nullptr;
};
