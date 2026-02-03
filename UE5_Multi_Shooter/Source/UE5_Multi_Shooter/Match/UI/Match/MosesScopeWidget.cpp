#include "UE5_Multi_Shooter/Match/UI/Match/MosesScopeWidget.h"

UMosesScopeWidget::UMosesScopeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesScopeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetScopeVisible(false);
}

void UMosesScopeWidget::SetScopeVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
