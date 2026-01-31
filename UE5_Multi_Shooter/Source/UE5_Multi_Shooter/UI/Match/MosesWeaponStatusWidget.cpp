// ============================================================================
// MosesWeaponStatusWidget.cpp (FULL)
// - CombatComponent(PS SSOT) Delegate 기반 무기/탄 UI 갱신
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesWeaponStatusWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

void UMosesWeaponStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedCombat = ResolveMyCombatComponent();
	if (CachedCombat)
	{
		// ✅ 존재하는 델리게이트만 바인딩
		CachedCombat->OnAmmoChanged.AddUObject(this, &UMosesWeaponStatusWidget::HandleAmmoChanged);
		CachedCombat->OnEquippedChanged.AddUObject(this, &UMosesWeaponStatusWidget::HandleEquippedChanged);

		// 초기 표시
		HandleEquippedChanged(CachedCombat->GetCurrentSlot(), CachedCombat->GetEquippedWeaponId());
		HandleAmmoChanged(CachedCombat->GetCurrentMagAmmo(), CachedCombat->GetCurrentReserveAmmo());
	}
}

UMosesCombatComponent* UMosesWeaponStatusWidget::ResolveMyCombatComponent() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	return PS ? PS->FindComponentByClass<UMosesCombatComponent>() : nullptr;
}

void UMosesWeaponStatusWidget::HandleAmmoChanged(int32 Mag, int32 Reserve)
{
	if (Text_Ammo)
	{
		Text_Ammo->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, Reserve)));
	}
}

void UMosesWeaponStatusWidget::HandleEquippedChanged(int32 /*SlotIndex*/, FGameplayTag WeaponId)
{
	if (Text_Weapon)
	{
		Text_Weapon->SetText(FText::FromString(WeaponId.IsValid() ? WeaponId.ToString() : TEXT("None")));
	}
}
