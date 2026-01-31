#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesOwnedSlotsWidget.generated.h"

class UTextBlock;
class UMosesSlotOwnershipComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesOwnedSlotsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

private:
	UMosesSlotOwnershipComponent* ResolveMySlotsComponent() const;
	void RefreshAll();

	// ✅ Delegate handlers (시그니처 맞추기)
	void HandleOwnedSlotsChanged();
	void HandleCurrentSlotChanged(int32 CurrentSlot);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Slots;

	UPROPERTY(Transient)
	TObjectPtr<UMosesSlotOwnershipComponent> CachedSlots;
};
