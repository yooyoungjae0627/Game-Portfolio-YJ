// ============================================================================
// MosesMatchAnnouncementWidget.cpp
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"

#include "Components/TextBlock.h"

// [MOD] FMosesAnnouncementState "정의"가 들어있는 헤더를 include 해야
//       State.bActive / State.Text 처럼 멤버 접근이 가능하다.
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

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
