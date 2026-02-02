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

	if (!AnnouncementText)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] AnnouncementText is NULL. Check WBP child TextBlock name == 'AnnouncementText'"));
		return;
	}

	// ---------------------------------------------------------------------
	// 2) 원본 텍스트 → FString
	// ---------------------------------------------------------------------
	FString TextStr = State.Text.ToString();

	// ---------------------------------------------------------------------
	// 3) 영어 문자 제거 (A-Z, a-z)
	// ---------------------------------------------------------------------
	for (int32 i = TextStr.Len() - 1; i >= 0; --i)
	{
		const TCHAR C = TextStr[i];
		if ((C >= TEXT('A') && C <= TEXT('Z')) ||
			(C >= TEXT('a') && C <= TEXT('z')))
		{
			TextStr.RemoveAt(i);
		}
	}

	// ---------------------------------------------------------------------
	// 4) 앞자리가 "한글"이 될 때까지 앞부분 제거
	//    한글 유니코드 범위: 0xAC00 ~ 0xD7A3
	// ---------------------------------------------------------------------
	while (TextStr.Len() > 0)
	{
		const TCHAR C = TextStr[0];
		if (C >= 0xAC00 && C <= 0xD7A3)
		{
			break; // ✅ 앞자리가 한글 → OK
		}

		// ❌ 한글 아니면 제거
		TextStr.RemoveAt(0);
	}

	// 앞뒤 공백 정리
	TextStr = TextStr.TrimStartAndEnd();

	AnnouncementText->SetText(FText::FromString(TextStr));

	// ---------------------------------------------------------------------
	// 5) 캡쳐 성공 판별 → 빨간색
	// ---------------------------------------------------------------------
	const bool bIsCaptureSuccess =
		TextStr.Contains(TEXT("캡쳐 성공")) ||
		TextStr.Contains(TEXT("캡처 성공")) ||
		TextStr.Contains(TEXT("깃발 캡처 성공"));

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
