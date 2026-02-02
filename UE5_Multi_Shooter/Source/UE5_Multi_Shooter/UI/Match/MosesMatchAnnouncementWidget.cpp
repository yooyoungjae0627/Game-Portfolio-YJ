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
	if (!AnnouncementText)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] AnnouncementText is NULL. Check WBP child TextBlock name == 'AnnouncementText'"));
		return;
	}

	AnnouncementText->SetText(State.Text);

	// ---------------------------------------------------------------------
	// [MOD] 캡쳐 성공은 무조건 빨간색으로 고정
	// - 다른 알림은 기본 White
	// - (원하면 여기서 "캡쳐 성공"만 보이고 나머지는 숨김도 가능)
	// ---------------------------------------------------------------------

	// ✅ "캡쳐 성공" / "Capture Complete" / "깃발 캡처 성공" 등 케이스 대응
	const FString TextStr = State.Text.ToString();
	const bool bIsCaptureSuccess =
		TextStr.Contains(TEXT("캡쳐 성공")) ||
		TextStr.Contains(TEXT("Capture Complete")) ||
		TextStr.Contains(TEXT("깃발 캡처 성공")) ||
		TextStr.Contains(TEXT("캡처 성공"));

	if (bIsCaptureSuccess)
	{
		AnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	}
	else
	{
		AnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
}

void UMosesMatchAnnouncementWidget::SetActive(bool bActive)
{
	SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
