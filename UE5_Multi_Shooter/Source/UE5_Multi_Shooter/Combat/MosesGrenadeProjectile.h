// ============================================================================
// MosesGrenadeProjectile.h (FULL)
// - 서버 폭발 판정 + RadialDamage 적용
// - 코스메틱은 Multicast로 처리
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesGrenadeProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGrenadeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMosesGrenadeProjectile();

protected:
	virtual void BeginPlay() override;

private:
	void Explode_Server();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExplodeCosmetics();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditDefaultsOnly, Category="Grenade")
	float FuseSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Grenade")
	float Damage = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category="Grenade")
	float Radius = 350.0f;

	FTimerHandle TimerHandle_Fuse;
};
