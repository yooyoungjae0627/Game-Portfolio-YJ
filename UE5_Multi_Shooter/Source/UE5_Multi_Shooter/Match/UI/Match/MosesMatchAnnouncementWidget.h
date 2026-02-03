// ============================================================================
// MosesMatchAnnouncementWidget.h
// - 방송(Announcement) 전용 자식 위젯
// - State에 따라 Visible/Collapsed 토글 + 텍스트 반영
// - 주의: FMosesAnnouncementState는 여기서는 전방선언만 하고,
//         .cpp에서 정의 헤더를 include하여 멤버 접근이 가능하게 한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesMatchAnnouncementWidget.generated.h"

class UTextBlock;

// [MOD] 전방선언 OK (함수 인자 "참조"까진 가능)
struct FMosesAnnouncementState;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesMatchAnnouncementWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

public:
	void UpdateAnnouncement(const FMosesAnnouncementState& State);

private:
	void SetActive(bool bActive);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AnnouncementText = nullptr;
};
