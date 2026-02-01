#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#include "MosesGrenadeProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class UAbilitySystemComponent;
class UMosesWeaponData;

/**
 * AMosesGrenadeProjectile
 *
 * [역할]
 * - 서버 권위 Projectile.
 * - Overlap 시 서버가 폭발 확정(원자성 1회)
 * - 폭발 시 OverlapSphere로 대상 수집 후:
 *   - GAS SetByCaller(Data.Damage)로 데미지 적용(히트스캔과 동일 파이프라인)
 *   - ASC 없는 대상은 ApplyDamage 폴백(최소 호환)
 * - FX/SFX는 Multicast로 전파(코스메틱)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGrenadeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMosesGrenadeProjectile();

public:
	/** 서버: 스폰 직후 초기화(데미지/반경/GE/SourceASC 등) */
	void InitFromCombat_Server(
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InDamageGE,
		float InDamage,
		float InRadius,
		AController* InInstigatorController,
		AActor* InDamageCauser,
		const UMosesWeaponData* InWeaponData);

	/** 서버: 발사 속도/방향 적용 */
	void Launch_Server(const FVector& Dir, float Speed);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void Explode_Server(const FVector& ExplodeLocation);

	void ApplyRadialDamage_Server(const FVector& Center);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayExplodeFX(const FVector& Center);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement = nullptr;

private:
	/** 폭발 원자성(서버 1회) */
	UPROPERTY(Replicated)
	bool bExploded = false;

private:
	/** 서버 런타임 캐시(복제 필요 없음) */
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	TSubclassOf<UGameplayEffect> DamageGE;

	float DamageAmount = 0.0f;
	float Radius = 450.0f;

	TWeakObjectPtr<AController> InstigatorController;
	TWeakObjectPtr<AActor> DamageCauser;

	/** 디버깅용(옵션) */
	TWeakObjectPtr<const UMosesWeaponData> WeaponData;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Grenade")
	float LifeSecondsAfterExplode = 1.0f;
};
