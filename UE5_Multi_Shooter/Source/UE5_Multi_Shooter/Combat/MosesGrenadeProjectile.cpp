// ============================================================================
// MosesGrenadeProjectile.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Combat/MosesGrenadeProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

AMosesGrenadeProjectile::AMosesGrenadeProjectile()
{
	bReplicates = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(10.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 1500.0f;
	Movement->MaxSpeed = 1500.0f;
	Movement->bRotationFollowsVelocity = true;
}

void AMosesGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(TimerHandle_Fuse, this, &AMosesGrenadeProjectile::Explode_Server, FuseSeconds, false);
	}
}

void AMosesGrenadeProjectile::Explode_Server()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesCombat, Log, TEXT("[GRENADE][SV] Explode Damage=%.1f Radius=%.1f Actor=%s"), Damage, Radius, *GetNameSafe(this));

	UGameplayStatics::ApplyRadialDamage(
		this,
		Damage,
		GetActorLocation(),
		Radius,
		nullptr,
		TArray<AActor*>(),
		this,
		GetInstigatorController(),
		true);

	MulticastExplodeCosmetics();
	Destroy();
}

void AMosesGrenadeProjectile::MulticastExplodeCosmetics_Implementation()
{
	UE_LOG(LogMosesCombat, Verbose, TEXT("[GRENADE][MC] ExplodeCosmetic Actor=%s"), *GetNameSafe(this));
}
