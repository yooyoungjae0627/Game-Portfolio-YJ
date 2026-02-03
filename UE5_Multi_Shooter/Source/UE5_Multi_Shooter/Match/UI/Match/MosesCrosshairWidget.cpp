// [MOD] includes 정리 (네 프로젝트 include 정렬 규칙에 맞춤)
#include "UE5_Multi_Shooter/Match/UI/Match/MosesCrosshairWidget.h"

#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Math/UnrealMathUtility.h"

// [MOD] 링크 에러(LNK2019) 해결: 헤더에 선언된 생성자 구현을 반드시 제공해야 한다.
UMosesCrosshairWidget::UMosesCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCrosshairWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// [MOD] 초기화 시점에 베이스 오프셋 캐시 + 수렴 상태로 시작
	CacheBaseOffsetsIfNeeded();
	SetSpreadFactor(0.0f);
}

void UMosesCrosshairWidget::CacheBaseOffsetsIfNeeded()
{
	if (bCachedBase)
	{
		return;
	}

	auto Cache = [](UImage* Img, FVector2D& OutPos)
		{
			if (!Img)
			{
				OutPos = FVector2D::ZeroVector;
				return;
			}

			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Img->Slot))
			{
				OutPos = CanvasSlot->GetPosition();
			}
			else
			{
				// CanvasPanelSlot이 아니면 Offset 제어 불가 → 0으로 방어
				OutPos = FVector2D::ZeroVector;
			}
		};

	// [MOD] 기획 이름(Img_Up/Down/Left/Right) 기준으로 캐시
	Cache(Img_Up, BaseUp);
	Cache(Img_Down, BaseDown);
	Cache(Img_Left, BaseLeft);
	Cache(Img_Right, BaseRight);

	bCachedBase = true;
}

void UMosesCrosshairWidget::ApplyOffset(UImage* Image, const FVector2D& BasePos, const FVector2D& AddPos)
{
	if (!Image)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
	{
		CanvasSlot->SetPosition(BasePos + AddPos);
	}
}

void UMosesCrosshairWidget::SetSpreadFactor(float InSpreadFactor01)
{
	CacheBaseOffsetsIfNeeded();

	const float Spread = FMath::Clamp(InSpreadFactor01, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(CachedSpread, Spread, 0.001f))
	{
		return;
	}

	CachedSpread = Spread;

	const float Pixels = FMath::Lerp(MinSpreadPixels, MaxSpreadPixels, Spread);

	// [MOD] 기획 이름(Img_*) 기준으로 4방향 이동
	ApplyOffset(Img_Up, BaseUp, FVector2D(0.0f, -Pixels));
	ApplyOffset(Img_Down, BaseDown, FVector2D(0.0f, Pixels));
	ApplyOffset(Img_Left, BaseLeft, FVector2D(-Pixels, 0.0f));
	ApplyOffset(Img_Right, BaseRight, FVector2D(Pixels, 0.0f));
}
