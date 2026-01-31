// ============================================================================
// MosesMatchBroadcastWidget.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesMatchBroadcastWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"

#include "UE5_Multi_Shooter/Match/MosesBroadcastComponent.h"

void UMosesMatchBroadcastWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedBroadcast = ResolveBroadcastComponent();
	if (CachedBroadcast)
	{
		CachedBroadcast->OnBroadcastChanged.AddUObject(this, &UMosesMatchBroadcastWidget::HandleBroadcastChanged);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UMosesMatchBroadcastWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_AutoHide);
	}

	Super::NativeDestruct();
}

UMosesBroadcastComponent* UMosesMatchBroadcastWidget::ResolveBroadcastComponent() const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? GS->FindComponentByClass<UMosesBroadcastComponent>() : nullptr;
}

void UMosesMatchBroadcastWidget::HandleBroadcastChanged(const FString& Message, float DisplaySeconds)
{
	if (Text_Broadcast)
	{
		Text_Broadcast->SetText(FText::FromString(Message));
	}

	SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogMosesMatch, Verbose, TEXT("[BROADCAST][CL] Show '%s' Sec=%.2f"), *Message, DisplaySeconds);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_AutoHide);
		World->GetTimerManager().SetTimer(
			TimerHandle_AutoHide,
			this,
			&UMosesMatchBroadcastWidget::HideSelf,
			FMath::Max(0.1f, DisplaySeconds),
			false);
	}
}

void UMosesMatchBroadcastWidget::HideSelf()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
