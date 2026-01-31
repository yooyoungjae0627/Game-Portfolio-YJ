// ============================================================================
// MosesPickupWeaponData.h (FULL)
// - 픽업 데이터(슬롯, ItemId, 월드 메쉬, 아이콘 등)
// - Actor는 이 데이터를 읽고, 서버 승인 시 PlayerState SSOT에 반영한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MosesPickupWeaponData.generated.h"

class UStaticMesh;
class UTexture2D;

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesPickupWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 슬롯 인덱스(1~4). 당신 기획에서 1~4(혹은 1~3)로 운용 가능.
	UPROPERTY(EditDefaultsOnly, Category="Pickup")
	int32 SlotIndex = 1;

	// 아이템 식별자(무기면 WeaponId로 사용 가능)
	UPROPERTY(EditDefaultsOnly, Category="Pickup")
	FGameplayTag ItemId;

	UPROPERTY(EditDefaultsOnly, Category="Pickup|Visual")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditDefaultsOnly, Category="Pickup|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};
