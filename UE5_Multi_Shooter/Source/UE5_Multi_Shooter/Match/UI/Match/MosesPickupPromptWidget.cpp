#include "UE5_Multi_Shooter/Match/UI/Match/MosesPickupPromptWidget.h"

#include "Components/TextBlock.h"

void UMosesPickupPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 기본 값은 비워둔다. (액터가 Overlap 시 SetPromptTexts로 세팅)
	if (TB_Interact)
	{
		TB_Interact->SetText(FText::GetEmpty());
	}

	if (TB_ItemName)
	{
		TB_ItemName->SetText(FText::GetEmpty());
	}
}

void UMosesPickupPromptWidget::SetPromptTexts(const FText& InInteractText, const FText& InItemNameText)
{
	if (TB_Interact)
	{
		TB_Interact->SetText(InInteractText);
	}

	if (TB_ItemName)
	{
		TB_ItemName->SetText(InItemNameText);
	}
}
