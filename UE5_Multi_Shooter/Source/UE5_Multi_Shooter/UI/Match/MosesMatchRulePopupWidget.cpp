#include "UE5_Multi_Shooter/UI/Match/MosesMatchRulePopupWidget.h"

#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"

void UMosesMatchRulePopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddDynamic(this, &ThisClass::HandleToggleClicked);
	}

	SetPopupVisible(false);

	if (RulesText)
	{
		RulesText->SetText(FText::FromString(
			TEXT("규칙 요약\n")
			TEXT("- Warmup: 준비/파밍/테스트\n")
			TEXT("- Combat: 서버 권위 진행\n")
			TEXT("- Result: 결과 표시 후 로비 복귀\n")
			TEXT("- HUD는 RepNotify/Delegate만 사용(Tick/Binding 금지)\n")));
	}
}

void UMosesMatchRulePopupWidget::HandleToggleClicked()
{
	bPopupVisible = !bPopupVisible;
	SetPopupVisible(bPopupVisible);
}

void UMosesMatchRulePopupWidget::SetPopupVisible(bool bVisible)
{
	bPopupVisible = bVisible;

	if (PopupRoot)
	{
		PopupRoot->SetVisibility(bPopupVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
