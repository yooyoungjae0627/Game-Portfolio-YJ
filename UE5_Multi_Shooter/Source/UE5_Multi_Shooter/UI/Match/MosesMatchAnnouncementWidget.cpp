#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"

#include "Components/TextBlock.h"

void UMosesMatchAnnouncementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetActive(false);
}

void UMosesMatchAnnouncementWidget::UpdateAnnouncement(const FMosesAnnouncementState& State)
{
	if (!AnnouncementText)
	{
		return;
	}

	if (!State.bActive)
	{
		AnnouncementText->SetText(FText::GetEmpty());
		SetActive(false);
		return;
	}

	AnnouncementText->SetText(State.Text);
	SetActive(true);
}

void UMosesMatchAnnouncementWidget::SetActive(bool bActive)
{
	SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
