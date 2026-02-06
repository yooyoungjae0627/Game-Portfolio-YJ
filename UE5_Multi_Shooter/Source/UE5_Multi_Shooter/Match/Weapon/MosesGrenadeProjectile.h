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
 * - 서버 권위 Projectile (Dedicated Server / Authority 100%)
 * - Overlap 기반 폭발 ❌ 금지
 * - Hit 기반 폭발 ✅ (OnComponentHit)
 * - 폭발 1회 원자성(bExploded)
 * - 폭발 시 OverlapSphere로 대상 수집 후:
 *   - GAS SetByCaller(Data.Damage)로 데미지 적용
 *   - ASC 없는 대상은 ApplyDamage 폴백
 * - FX/SFX는 Multicast로 통일(코스메틱)
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
	/** [MOD] Overlap 금지 → Hit 기반으로만 폭발 */
	UFUNCTION()
	void OnCollisionHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	/** 서버만 */
	void Explode_Server(const FVector& ExplodeLocation, const FHitResult* HitOpt);

	/** 서버만 */
	void ApplyRadialDamage_Server(const FVector& Center);

	/** [MOD] 서버 스폰 직후 Self-hit 방지용 Ignore 설정 */
	void ConfigureIgnoreActors_Server();

	/** [MOD] Arming(스폰 직후 짧은 시간 충돌 무시) */
	bool IsArming_Server() const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayExplodeFX(const FVector& Center, const FVector& HitNormal);

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
	/** 폭발 후 몇 초 뒤 제거(연출 여유) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Grenade")
	float LifeSecondsAfterExplode = 1.0f;

	/** [MOD] 스폰 직후 self-hit 방지 arming 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Grenade")
	float ArmingSeconds = 0.06f;

	/** [MOD] 서버에서 스폰된 시간(arming 판정용) */
	float SpawnWorldTimeSeconds_Server = 0.0f;
};
