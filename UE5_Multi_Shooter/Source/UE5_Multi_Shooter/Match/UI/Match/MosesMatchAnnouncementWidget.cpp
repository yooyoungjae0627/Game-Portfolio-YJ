// ============================================================================
// MosesMatchAnnouncementWidget.cpp (FULL)
// - UpdateAnnouncement: Visible/Collapsed + 텍스트 반영
// - [MOD] 현재 MatchPhase를 조회해서 색상 적용
// ============================================================================

#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchAnnouncementWidget.h"

#include "Components/TextBlock.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "Engine/World.h"

void UMosesMatchAnnouncementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 기본은 꺼둠 (서버가 AnnouncementState를 켜줄 때만 보인다)
	SetActive(false);
}

void UMosesMatchAnnouncementWidget::UpdateAnnouncement(const FMosesAnnouncementState& State)
{
	SetActive(State.bActive);

	if (!State.bActive)
	{
		return;
	}

	if (!AnnouncementText)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] AnnouncementText is NULL. Check WBP child TextBlock name == 'AnnouncementText'"));
		return;
	}

	FString TextStr = State.Text.ToString();

	for (int32 i = TextStr.Len() - 1; i >= 0; --i)
	{
		const TCHAR C = TextStr[i];
		if ((C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z')))
		{
			TextStr.RemoveAt(i);
		}
	}

	while (TextStr.Len() > 0)
	{
		const TCHAR C = TextStr[0];
		if (C >= 0xAC00 && C <= 0xD7A3)
		{
			break;
		}
		TextStr.RemoveAt(0);
	}

	TextStr = TextStr.TrimStartAndEnd();
	AnnouncementText->SetText(FText::FromString(TextStr));

	// ---------------------------------------------------------------------
	// [MOD] 헤드샷/캡처 성공이면 빨간색
	// ---------------------------------------------------------------------
	const bool bIsHeadshot =
		TextStr.Contains(TEXT("헤드샷")) ||
		TextStr.Contains(TEXT("헤드 샷"));

	const bool bIsCaptureSuccess =
		TextStr.Contains(TEXT("캡쳐 성공")) ||
		TextStr.Contains(TEXT("캡처 성공")) ||
		TextStr.Contains(TEXT("깃발 캡처 성공"));

	if (bIsHeadshot || bIsCaptureSuccess)
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
	if (bActive)
	{
		if (GetVisibility() == ESlateVisibility::SelfHitTestInvisible)
		{
			return;
		}

		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		if (GetVisibility() == ESlateVisibility::Collapsed)
		{
			return;
		}

		SetVisibility(ESlateVisibility::Collapsed);
	}
}
