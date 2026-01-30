#include "UE5_Multi_Shooter/UI/Match/MosesMatchRulePopupWidget.h"

#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UMosesMatchRulePopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddDynamic(this, &ThisClass::HandleToggleClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}

	SetPopupVisible(false);

	if (RulesText)
	{
		RulesText->SetText(FText::FromString(
			TEXT("규칙 요약\n")
			TEXT("- Warmup: 준비/파밍/테스트\n")
			TEXT("- Match: 깃발 3초 유지, 피격/이탈/사망 시 취소\n")
			TEXT("- Result: 결과 표시 후 입력 제한\n")
			TEXT("- 서버 권위 100% / HUD는 RepNotify->Delegate만 사용(Tick/Binding 금지)\n")));
	}
}

void UMosesMatchRulePopupWidget::HandleToggleClicked()
{
	SetPopupVisible(!bPopupVisible);
}

void UMosesMatchRulePopupWidget::HandleCloseClicked()
{
	SetPopupVisible(false);
}

void UMosesMatchRulePopupWidget::SetPopupVisible(bool bVisible)
{
	bPopupVisible = bVisible;

	if (PopupRoot)
	{
		PopupRoot->SetVisibility(bPopupVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	ApplyInputModeForPopup(bPopupVisible);
}

void UMosesMatchRulePopupWidget::ApplyInputModeForPopup(bool bInPopupVisible)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (bInPopupVisible)
	{
		// 팝업이 떠 있을 때는 UI 조작이 최우선.
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		// 팝업이 닫히면 게임 입력으로 복귀.
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(false);
	}
}
