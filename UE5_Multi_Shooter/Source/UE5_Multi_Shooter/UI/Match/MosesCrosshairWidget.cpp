// ============================================================================
// MosesCrosshairWidget.cpp (FULL)
// - 로컬 코스메틱: Pawn 속도 기반 크로스헤어 확산/수렴
// - Tick 금지: Timer로 갱신
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesCrosshairWidget.h"

#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

void UMosesCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerHandle_Update,
			this,
			&UMosesCrosshairWidget::UpdateCrosshair_Local,
			UpdateInterval,
			true);
	}
}

void UMosesCrosshairWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_Update);
	}

	Super::NativeDestruct();
}

float UMosesCrosshairWidget::ComputeSpreadFactor_Local() const
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return 0.0f;
	}

	const float Speed = Pawn->GetVelocity().Size();
	return FMath::Clamp(Speed / 600.0f, 0.0f, 1.0f);
}

void UMosesCrosshairWidget::SetImageOffset(UImage* Img, float X, float Y) const
{
	if (!Img)
	{
		return;
	}

	// ✅ 'Slot' 이름은 UWidget::Slot 멤버를 가리므로 CanvasSlot로 변경
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Img->Slot))
	{
		CanvasSlot->SetPosition(FVector2D(X, Y));
	}
}

void UMosesCrosshairWidget::UpdateCrosshair_Local()
{
	const float Spread = ComputeSpreadFactor_Local();
	const float Offset = FMath::Lerp(MinOffset, MaxOffset, Spread);

	SetImageOffset(Img_Top, 0.0f, -Offset);
	SetImageOffset(Img_Bottom, 0.0f, Offset);
	SetImageOffset(Img_Left, -Offset, 0.0f);
	SetImageOffset(Img_Right, Offset, 0.0f);

	UE_LOG(LogMosesHUD, VeryVerbose, TEXT("[HUD][CL] Crosshair Spread=%.2f Offset=%.1f"), Spread, Offset);
}
