#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MosesWeaponStatusWidget.generated.h"

class UTextBlock;
class UMosesCombatComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesWeaponStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

private:
	UMosesCombatComponent* ResolveMyCombatComponent() const;

	// ✅ CombatComponent에 실제 존재하는 델리게이트에 맞춘 핸들러
	void HandleAmmoChanged(int32 Mag, int32 Reserve);
	void HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Weapon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Ammo;

	UPROPERTY(Transient)
	TObjectPtr<UMosesCombatComponent> CachedCombat;
};
