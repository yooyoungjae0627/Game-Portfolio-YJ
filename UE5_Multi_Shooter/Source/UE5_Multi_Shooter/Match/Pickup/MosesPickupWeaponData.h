// ============================================================================
// MosesPickupWeaponData.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// 역할
// - 픽업(무기) 데이터 에셋.
// - 오늘 목표(월드 말풍선 + Announcement)에 필요한 최소 필드 포함.
//
// 주의
// - 말풍선/방송 텍스트는 Tick/Binding 없이, 액터/서버 로직에서 SetText로만 사용.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MosesPickupWeaponData.generated.h"

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesPickupWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// World presentation
	// -------------------------------------------------------------------------

	/** 월드에 표시할 정적 메시(선택) */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|World")
	TSoftObjectPtr<USkeletalMesh> WorldMesh;

	// -------------------------------------------------------------------------
	// Prompt UI (World Widget)
	// -------------------------------------------------------------------------

	/** [MOD] 말풍선에 표시할 아이템 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Prompt")
	FText DisplayName;

	/** [MOD] 말풍선에 표시할 상호작용 안내 (예: "E : 줍기") */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Prompt")
	FText InteractText;

	// -------------------------------------------------------------------------
	// Grant payload
	// -------------------------------------------------------------------------

	/** 지급할 슬롯 인덱스 (프로젝트 정책에 맞춰 유지) */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Grant")
	int32 SlotIndex = 1;

	/** 지급할 아이템/무기 ID */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Grant")
	FGameplayTag ItemId;
};
