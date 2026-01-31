// ============================================================================
// MosesMatchAnnouncementWidget.cpp (FULL)
// - UpdateAnnouncement: Visible/Collapsed + 텍스트 반영
// - [MOD] 현재 MatchPhase를 조회해서 색상 적용
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"

#include "Components/TextBlock.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "Engine/World.h"

void UMosesMatchAnnouncementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 기본은 꺼둠 (서버가 AnnouncementState를 켜줄 때만 보인다)
	SetActive(false);
}

void UMosesMatchAnnouncementWidget::UpdateAnnouncement(const FMosesAnnouncementState& State)
{
	// 1) Active 토글
	SetActive(State.bActive);

	if (!State.bActive)
	{
		return;
	}

	// 2) Text 반영
	if (AnnouncementText)
	{
		AnnouncementText->SetText(State.Text);

		// [MOD] Phase 기준 색상 정책
		// - Warmup / Result : White
		// - Combat          : Red
		const AMosesMatchGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
		const EMosesMatchPhase Phase = GS ? GS->GetMatchPhase() : EMosesMatchPhase::Warmup;

		if (Phase == EMosesMatchPhase::Combat)
		{
			AnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
		}
		else
		{
			AnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
	else
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] AnnouncementText is NULL. Check WBP child TextBlock name == 'AnnouncementText'"));
	}
}

void UMosesMatchAnnouncementWidget::SetActive(bool bActive)
{
	SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
