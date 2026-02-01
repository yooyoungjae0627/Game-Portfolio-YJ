#include "UE5_Multi_Shooter/UI/Match/MosesCrosshairWidget.h"

#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Math/UnrealMathUtility.h"

UMosesCrosshairWidget::UMosesCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesCrosshairWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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

			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Img->Slot)) // ✅ FIX: Slot -> CanvasSlot
			{
				OutPos = CanvasSlot->GetPosition();
			}
			else
			{
				OutPos = FVector2D::ZeroVector;
			}
		};

	Cache(Crosshair_Up, BaseUp);
	Cache(Crosshair_Down, BaseDown);
	Cache(Crosshair_Left, BaseLeft);
	Cache(Crosshair_Right, BaseRight);

	bCachedBase = true;
}

void UMosesCrosshairWidget::ApplyOffset(UImage* Image, const FVector2D& BasePos, const FVector2D& AddPos)
{
	if (!Image)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot)) // ✅ FIX: Slot -> CanvasSlot
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

	ApplyOffset(Crosshair_Up, BaseUp, FVector2D(0.0f, -Pixels));
	ApplyOffset(Crosshair_Down, BaseDown, FVector2D(0.0f, Pixels));
	ApplyOffset(Crosshair_Left, BaseLeft, FVector2D(-Pixels, 0.0f));
	ApplyOffset(Crosshair_Right, BaseRight, FVector2D(Pixels, 0.0f));
}
