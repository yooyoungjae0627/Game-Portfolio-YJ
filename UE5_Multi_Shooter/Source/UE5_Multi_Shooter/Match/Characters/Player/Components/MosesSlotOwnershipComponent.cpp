#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesSlotOwnershipComponent.h"

UMosesSlotOwnershipComponent::UMosesSlotOwnershipComponent()
{
	SetIsReplicatedByDefault(true);

	// 1..4 슬롯을 고정 크기로 운용(인덱스 보정 위해 4로 초기화)
	SlotItemIds.SetNum(4);
}

void UMosesSlotOwnershipComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesSlotOwnershipComponent, OwnedSlotsMask);
	DOREPLIFETIME(UMosesSlotOwnershipComponent, CurrentSlot);
	DOREPLIFETIME(UMosesSlotOwnershipComponent, SlotItemIds);
}

void UMosesSlotOwnershipComponent::ServerAcquireSlot(int32 SlotIndex, const FGameplayTag& ItemId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 ClampedSlot = FMath::Clamp(SlotIndex, 1, 4);
	const int32 Bit = (1 << (ClampedSlot - 1));

	OwnedSlotsMask |= Bit;

	SlotItemIds[ClampedSlot - 1] = ItemId;

	UE_LOG(LogMosesPickup, Log, TEXT("[SLOTS][SV] Acquire Slot=%d Item=%s Mask=%d"),
		ClampedSlot, *ItemId.ToString(), OwnedSlotsMask);

	BroadcastOwnedSlots();
}

void UMosesSlotOwnershipComponent::ServerSetCurrentSlot(int32 NewSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 ClampedSlot = FMath::Clamp(NewSlotIndex, 1, 4);
	if (!HasSlot(ClampedSlot))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[SLOTS][SV] SetCurrentSlot REJECT NotOwned Slot=%d Mask=%d"),
			ClampedSlot, OwnedSlotsMask);
		return;
	}

	if (CurrentSlot == ClampedSlot)
	{
		return;
	}

	CurrentSlot = ClampedSlot;

	UE_LOG(LogMosesCombat, Log, TEXT("[SLOTS][SV] CurrentSlot=%d"), CurrentSlot);

	BroadcastCurrentSlot();
}

bool UMosesSlotOwnershipComponent::HasSlot(int32 SlotIndex) const
{
	const int32 ClampedSlot = FMath::Clamp(SlotIndex, 1, 4);
	const int32 Bit = (1 << (ClampedSlot - 1));
	return (OwnedSlotsMask & Bit) != 0;
}

FGameplayTag UMosesSlotOwnershipComponent::GetItemIdForSlot(int32 SlotIndex) const
{
	const int32 ClampedSlot = FMath::Clamp(SlotIndex, 1, 4);
	return SlotItemIds.IsValidIndex(ClampedSlot - 1) ? SlotItemIds[ClampedSlot - 1] : FGameplayTag();
}

void UMosesSlotOwnershipComponent::OnRep_OwnedSlotsMask()
{
	UE_LOG(LogMosesPickup, Verbose, TEXT("[SLOTS][CL] OnRep OwnedMask=%d"), OwnedSlotsMask);
	BroadcastOwnedSlots();
}

void UMosesSlotOwnershipComponent::OnRep_CurrentSlot()
{
	UE_LOG(LogMosesCombat, Verbose, TEXT("[SLOTS][CL] OnRep CurrentSlot=%d"), CurrentSlot);
	BroadcastCurrentSlot();
}

void UMosesSlotOwnershipComponent::OnRep_SlotItemIds()
{
	UE_LOG(LogMosesPickup, Verbose, TEXT("[SLOTS][CL] OnRep SlotItemIds"));
	BroadcastOwnedSlots();
}

void UMosesSlotOwnershipComponent::BroadcastOwnedSlots()
{
	OnOwnedSlotsChanged.Broadcast();
}

void UMosesSlotOwnershipComponent::BroadcastCurrentSlot()
{
	OnCurrentSlotChanged.Broadcast(CurrentSlot);
}
