#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MosesZombieTypeData.generated.h"

class UAnimMontage;
class USoundBase;
class UGameplayEffect;

/**
 * Zombie Type Data (Data-only, GF_Combat_Data 권장)
 * - 좀비 타입별 스펙을 DataAsset로 분리한다.
 * - "피해 적용"은 반드시 GAS GameplayEffect로 통일한다.
 *
 * Server Authority:
 * - Overlap/Trace 판정: 서버
 * - 피해/HP 감소: 서버가 GE Apply
 * - 죽음/Destroy: 서버
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesZombieTypeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UMosesZombieTypeData();

public:
	/** Max HP (Default 100) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Stats")
	float MaxHP;

	/** Melee Damage Magnitude (Default 10) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Combat")
	float MeleeDamage;

	/**
	 * Damage GameplayEffect (SetByCaller)
	 * - 프로젝트 표준: GE_Damage_SetByCaller
	 * - Tag: Data.Damage
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Combat")
	TSubclassOf<UGameplayEffect> DamageGE_SetByCaller;

	/**
	 * Attack Montages (2개 권장)
	 * - 서버가 랜덤 선택 -> Multicast로 재생(코스메틱)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Combat")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	/** Headshot Kill Sound (로컬 연출) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Feedback")
	TObjectPtr<USoundBase> HeadshotKillSound;
};
