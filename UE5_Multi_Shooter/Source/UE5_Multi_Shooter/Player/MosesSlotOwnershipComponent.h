// ============================================================================
// MosesSlotOwnershipComponent.h (FULL)
// - PlayerState(SSOT) 슬롯 소유/현재 슬롯/슬롯별 ItemId를 RepNotify로 복제한다.
// - HUD는 Native Delegate로 갱신한다.
// - 픽업 성공 시 서버에서만 값이 갱신된다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesSlotOwnershipComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnMosesOwnedSlotsChangedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesCurrentSlotChangedNative, int32 /*CurrentSlot*/);

UCLASS(ClassGroup=(Moses), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesSlotOwnershipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesSlotOwnershipComponent();

	// 서버: 슬롯 획득(픽업 성공)
	void ServerAcquireSlot(int32 SlotIndex, const FGameplayTag& ItemId);

	// 서버: 현재 슬롯 변경(키 1~4)
	void ServerSetCurrentSlot(int32 NewSlotIndex);

	// Query
	bool HasSlot(int32 SlotIndex) const;
	int32 GetCurrentSlot() const { return CurrentSlot; }
	FGameplayTag GetItemIdForSlot(int32 SlotIndex) const;

	// HUD delegates
	FOnMosesOwnedSlotsChangedNative OnOwnedSlotsChanged;
	FOnMosesCurrentSlotChangedNative OnCurrentSlotChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// RepNotifies
	UFUNCTION()
	void OnRep_OwnedSlotsMask();

	UFUNCTION()
	void OnRep_CurrentSlot();

	UFUNCTION()
	void OnRep_SlotItemIds();

	void BroadcastOwnedSlots();
	void BroadcastCurrentSlot();

	// Bitmask: bit0=slot1, bit1=slot2...
	UPROPERTY(ReplicatedUsing=OnRep_OwnedSlotsMask)
	int32 OwnedSlotsMask = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentSlot)
	int32 CurrentSlot = 1;

	// SlotIndex(1..4) -> ItemId
	UPROPERTY(ReplicatedUsing=OnRep_SlotItemIds)
	TArray<FGameplayTag> SlotItemIds; // size=4 권장
};
