// ============================================================================
// MosesOwnedSlotsWidget.cpp (FULL)
// - SlotOwnershipComponent(PS SSOT) Delegate 기반 UI 갱신
// ============================================================================

#include "UE5_Multi_Shooter/Match/UI/Match/MosesOwnedSlotsWidget.h"
#include "UE5_Multi_Shooter/Match/Components/MosesSlotOwnershipComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UMosesOwnedSlotsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedSlots = ResolveMySlotsComponent();
	if (CachedSlots)
	{
		CachedSlots->OnOwnedSlotsChanged.AddUObject(this, &UMosesOwnedSlotsWidget::HandleOwnedSlotsChanged);

		// ✅ OnCurrentSlotChanged는 (int32) 시그니처 → 전용 핸들러로 바인딩
		CachedSlots->OnCurrentSlotChanged.AddUObject(this, &UMosesOwnedSlotsWidget::HandleCurrentSlotChanged);
	}

	RefreshAll();
}

UMosesSlotOwnershipComponent* UMosesOwnedSlotsWidget::ResolveMySlotsComponent() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	return PS ? PS->FindComponentByClass<UMosesSlotOwnershipComponent>() : nullptr;
}

void UMosesOwnedSlotsWidget::HandleOwnedSlotsChanged()
{
	RefreshAll();
}

void UMosesOwnedSlotsWidget::HandleCurrentSlotChanged(int32 /*CurrentSlot*/)
{
	RefreshAll();
}

void UMosesOwnedSlotsWidget::RefreshAll()
{
	if (!Text_Slots || !CachedSlots)
	{
		return;
	}

	const int32 Current = CachedSlots->GetCurrentSlot();

	FString S;
	for (int32 SlotIndex = 1; SlotIndex <= 4; ++SlotIndex) // ✅ Slot -> SlotIndex (C4458 방지)
	{
		const bool bOwned = CachedSlots->HasSlot(SlotIndex);
		S += FString::Printf(TEXT("[%d:%s%s] "),
			SlotIndex,
			bOwned ? TEXT("ON") : TEXT("OFF"),
			(SlotIndex == Current) ? TEXT("*") : TEXT(""));
	}

	Text_Slots->SetText(FText::FromString(S));
	UE_LOG(LogMosesHUD, VeryVerbose, TEXT("[HUD][CL] OwnedSlots Updated: %s"), *S);
}
