// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h
// (FULL - UPDATED)
// - AttackMontages + [NEW] DyingMontage + [NEW] DeathDestroyDelaySeconds
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MosesZombieTypeData.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * Zombie Type Data (DataAsset)
 * - Stats + Attack Montages + Death Montage + (AI Params)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesZombieTypeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UMosesZombieTypeData();

public:
	// ---- Combat ----
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Combat")
	float MaxHP = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Combat")
	float MeleeDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Combat")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	// -------------------------
	// [NEW] Death Montage
	// -------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Combat")
	TObjectPtr<UAnimMontage> DyingMontage = nullptr; // [NEW]

	// -------------------------
	// [NEW] Death Destroy Delay
	// - 0이면 Montage 길이 기반으로 자동
	// - 0보다 크면 이 값을 우선 사용
	// -------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Combat", meta = (ClampMin = "0.0"))
	float DeathDestroyDelaySeconds = 0.f; // [NEW]

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Combat")
	TSubclassOf<UGameplayEffect> DamageGE_SetByCaller;

	// ---- Headshot Popup ----
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Headshot")
	FText HeadshotAnnouncementText;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|Headshot")
	float HeadshotAnnouncementSeconds = 0.9f;

	// -------------------------
	// AI Params (Server)
	// -------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Sense")
	float SightRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Sense")
	float LoseSightRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Sense")
	float PeripheralVisionAngleDeg = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Move")
	float AcceptanceRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Move")
	float MaxChaseDistance = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Attack")
	float AttackRange = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Zombie|AI|Attack")
	float AttackCooldownSeconds = 1.2f;
};
