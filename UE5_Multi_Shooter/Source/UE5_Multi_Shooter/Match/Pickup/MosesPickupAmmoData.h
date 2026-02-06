// ============================================================================
// MosesPickupAmmoData.h (FULL)
// ----------------------------------------------------------------------------
// 역할
// - 탄약 픽업 데이터 에셋.
// - BP에는 증가 로직 없음. (서버에서만 CombatComponent가 증가)
// - Pickup은 AmmoTypeId 기준으로 "해당 타입 무기 존재" 슬롯에 적용.
//
// 필드
// - AmmoTypeId: Ammo.Rifle / Ammo.Sniper / Ammo.Grenade
// - ReserveMaxDelta: ReserveMax 증가량
// - ReserveFillDelta: ReserveCurrent 증가량(Clamp)
// - DisplayName/InteractText: 월드 프롬프트/Announcement 용
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MosesPickupAmmoData.generated.h"

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesPickupAmmoData : public UDataAsset
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// Prompt/UI
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category="Pickup|Prompt")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, Category="Pickup|Prompt")
	FText InteractText;

	// -------------------------------------------------------------------------
	// Payload
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category="Pickup|Ammo")
	FGameplayTag AmmoTypeId; // Ammo.Rifle / Ammo.Sniper / Ammo.Grenade

	UPROPERTY(EditDefaultsOnly, Category="Pickup|Ammo", meta=(ClampMin="0"))
	int32 ReserveMaxDelta = 0;

	UPROPERTY(EditDefaultsOnly, Category="Pickup|Ammo", meta=(ClampMin="0"))
	int32 ReserveFillDelta = 0;

	// -------------------------------------------------------------------------
	// World presentation (선택)
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category="Pickup|World")
	TSoftObjectPtr<class UStaticMesh> WorldMesh;
};
