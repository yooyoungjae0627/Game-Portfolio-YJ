// ============================================================================
// MosesMatchTypes.h (FULL)
// - Match 전용 데이터 구조체(USTRUCT) 모음
// - HUD는 RepNotify -> Delegate 파이프라인으로만 갱신한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"

#include "MosesMatchTypes.generated.h"

/**
 * FMosesAnnouncementState
 *
 * - 서버 권위 100% 정책: 방송 상태는 서버가 단일 진실로 확정한다.
 * - HUD 정책: Tick/Binding 금지. RepNotify -> Delegate -> Widget Update 로만 갱신한다.
 * - 타이밍 정책: 클라에서 타이머를 돌리지 않는다. 서버가 RemainingSeconds를 1초씩 갱신하여 복제한다.
 */
USTRUCT(BlueprintType)
struct FMosesAnnouncementState
{
	GENERATED_BODY()

public:
	/** 방송 표시 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bActive = false;

	/** 카운트다운 형식 여부 (Text가 매 초 갱신됨) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bCountdown = false;

	/** 현재 표시 텍스트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText Text;

	/** 서버가 확정하는 남은 초 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RemainingSeconds = 0;

public:
	bool operator==(const FMosesAnnouncementState& Other) const
	{
		return bActive == Other.bActive
			&& bCountdown == Other.bCountdown
			&& Text.EqualTo(Other.Text)
			&& RemainingSeconds == Other.RemainingSeconds;
	}

	bool operator!=(const FMosesAnnouncementState& Other) const
	{
		return !(*this == Other);
	}
};
