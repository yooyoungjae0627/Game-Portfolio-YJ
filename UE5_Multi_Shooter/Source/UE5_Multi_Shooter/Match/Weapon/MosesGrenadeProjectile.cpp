#include "MosesGrenadeProjectile.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/EngineTypes.h"

#include "Kismet/GameplayStatics.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

AMosesGrenadeProjectile::AMosesGrenadeProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true); // [MOD] 투사체 이동 복제 안정화

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);

	// ----------------------------
	// [MOD] HIT 기반 세팅(Overlap 금지)
	// ----------------------------
	Collision->InitSphereRadius(12.0f);

	// ProjectileMovement는 "Block/Hit"가 정석이라 QueryAndPhysics 권장
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // [MOD]
	Collision->SetCollisionObjectType(ECC_WorldDynamic);

	// 전부 Block 기본, Camera/Visibility는 Ignore
	Collision->SetCollisionResponseToAllChannels(ECR_Block); // [MOD]
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // [MOD]
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); // [MOD]

	// Overlap 이벤트 금지
	Collision->SetGenerateOverlapEvents(false); // [MOD]

	// Hit 이벤트 발생(= "Simulation Generates Hit Events" 의미)
	Collision->SetNotifyRigidBodyCollision(true); // [MOD]

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision); // [MOD] UpdatedComponent 명시

	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.35f;
	Movement->InitialSpeed = 2400.0f;
	Movement->MaxSpeed = 3200.0f;

	// Bounce는 BP/데이터에서 바꿀 수 있게 기본 OFF 유지(원하면 true)
	Movement->bShouldBounce = false;
}

void AMosesGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SpawnWorldTimeSeconds_Server = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f; // [MOD]
	}

	if (Collision)
	{
		// [MOD] Overlap 바인딩 삭제 → Hit 바인딩으로 교체
		Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnCollisionHit);
	}
}

void AMosesGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMosesGrenadeProjectile, bExploded);
}

void AMosesGrenadeProjectile::InitFromCombat_Server(
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InDamageGE,
	float InDamage,
	float InRadius,
	AController* InInstigatorController,
	AActor* InDamageCauser,
	const UMosesWeaponData* InWeaponData)
{
	check(HasAuthority());

	SourceASC = InSourceASC;
	DamageGE = InDamageGE;
	DamageAmount = FMath::Max(0.0f, InDamage);
	Radius = FMath::Max(10.0f, InRadius);

	InstigatorController = InInstigatorController;
	DamageCauser = InDamageCauser;
	WeaponData = InWeaponData;

	ConfigureIgnoreActors_Server(); // [MOD] self-hit 방지
}

void AMosesGrenadeProjectile::Launch_Server(const FVector& Dir, float Speed)
{
	check(HasAuthority());

	if (Movement)
	{
		const FVector Vel = Dir.GetSafeNormal() * FMath::Max(1.0f, Speed);
		Movement->Velocity = Vel;
		Movement->Activate(true); // [MOD] 명시적으로 활성화
	}
}

void AMosesGrenadeProjectile::ConfigureIgnoreActors_Server()
{
	check(HasAuthority());

	// [MOD] 발사자(Owner/Instigator) 즉시 자폭 방지
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && Collision)
	{
		Collision->IgnoreActorWhenMoving(OwnerActor, true);
	}

	APawn* InstPawn = (InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr);
	if (InstPawn && Collision)
	{
		Collision->IgnoreActorWhenMoving(InstPawn, true);
	}

	if (AActor* Causer = DamageCauser.Get())
	{
		if (Collision)
		{
			Collision->IgnoreActorWhenMoving(Causer, true);
		}
	}

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[GRENADE][SV] ConfigureIgnore Owner=%s InstPawn=%s Causer=%s"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(InstPawn),
		*GetNameSafe(DamageCauser.Get()));
}

bool AMosesGrenadeProjectile::IsArming_Server() const
{
	if (!HasAuthority())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	return (Now - SpawnWorldTimeSeconds_Server) < ArmingSeconds;
}

void AMosesGrenadeProjectile::OnCollisionHit(
	UPrimitiveComponent* /*HitComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/,
	const FHitResult& Hit)
{
	// OnHit는 모든 머신에서 호출될 수 있음 → 서버만 결정
	if (!HasAuthority())
	{
		return;
	}

	if (bExploded)
	{
		return;
	}

	// [MOD] 스폰 직후 짧은 시간은 충돌 무시(총구/캡슐 겹침 보호)
	if (IsArming_Server())
	{
		UE_LOG(LogMosesCombat, Verbose,
			TEXT("[GRENADE][SV] Hit Ignored (Arming) Other=%s"),
			*GetNameSafe(OtherActor));
		return;
	}

	// [MOD] 자기 자신/Owner/Instigator 충돌은 무시(2중 방어)
	AActor* OwnerActor = GetOwner();
	APawn* InstPawn = (InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr);

	if (OtherActor == nullptr
		|| OtherActor == this
		|| OtherActor == OwnerActor
		|| OtherActor == InstPawn)
	{
		UE_LOG(LogMosesCombat, Verbose,
			TEXT("[GRENADE][SV] Hit Ignored (Self/Owner/Instigator) Other=%s"),
			*GetNameSafe(OtherActor));
		return;
	}

	Explode_Server(Hit.ImpactPoint, &Hit);
}

void AMosesGrenadeProjectile::Explode_Server(const FVector& ExplodeLocation, const FHitResult* HitOpt)
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}

	bExploded = true;

	const FVector HitNormal = (HitOpt ? HitOpt->ImpactNormal : FVector::UpVector);

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[GRENADE][SV] Explode Loc=%s Normal=%s Radius=%.1f Damage=%.1f Weapon=%s"),
		*ExplodeLocation.ToCompactString(),
		*HitNormal.ToCompactString(),
		Radius,
		DamageAmount,
		WeaponData.IsValid() ? *WeaponData->WeaponId.ToString() : TEXT("None"));

	ApplyRadialDamage_Server(ExplodeLocation);

	// 코스메틱은 멀티캐스트로 통일
	Multicast_PlayExplodeFX(ExplodeLocation, HitNormal);

	// 연출 여유 후 제거
	SetLifeSpan(LifeSecondsAfterExplode);
}

void AMosesGrenadeProjectile::ApplyRadialDamage_Server(const FVector& Center)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Results;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(Moses_GrenadeOverlap), false);
	Params.AddIgnoredActor(this);

	if (AActor* Causer = DamageCauser.Get())
	{
		Params.AddIgnoredActor(Causer);
	}

	// Owner/Instigator도 피해 제외(자기 자신/발사자 보호)
	if (AActor* OwnerActor = GetOwner())
	{
		Params.AddIgnoredActor(OwnerActor);
	}
	if (APawn* InstPawn = (InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr))
	{
		Params.AddIgnoredActor(InstPawn);
	}

	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);

	// 대상 수집은 Pawn 채널로(플레이어/좀비)
	const bool bAny = World->OverlapMultiByChannel(
		Results,
		Center,
		FQuat::Identity,
		ECC_Pawn,
		Sphere,
		Params);

	int32 HitCount = 0;

	for (const FOverlapResult& R : Results)
	{
		AActor* Target = R.GetActor();
		if (!Target)
		{
			continue;
		}

		bool bAppliedByGAS = false;

		// GAS 통일 적용
		if (UAbilitySystemComponent* SrcASC = SourceASC.Get())
		{
			if (Target->GetClass()->ImplementsInterface(UAbilitySystemInterface::StaticClass()))
			{
				IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(Target);
				UAbilitySystemComponent* TargetASC = TargetASI ? TargetASI->GetAbilitySystemComponent() : nullptr;

				if (TargetASC && DamageGE)
				{
					FGameplayEffectContextHandle Ctx = SrcASC->MakeEffectContext();

					// [MOD] Instigator/Causer 정리:
					// - Instigator: 발사자 Pawn/Controller
					// - EffectCauser: Projectile(this) 권장
					APawn* InstPawn = (InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr);
					Ctx.AddInstigator(InstPawn, InstigatorController.Get());
					Ctx.AddSourceObject(this);

					if (WeaponData.IsValid())
					{
						Ctx.AddSourceObject(WeaponData.Get());
					}

					const FGameplayEffectSpecHandle SpecHandle = SrcASC->MakeOutgoingSpec(DamageGE, 1.f, Ctx);
					if (SpecHandle.IsValid())
					{
						// 데미지는 음수로(기존 파이프라인 유지)
						const float SignedDamage = -FMath::Abs(DamageAmount);
						SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Damage, SignedDamage);

						SrcASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

						bAppliedByGAS = true;

						UE_LOG(LogMosesGAS, Warning,
							TEXT("[DAMAGE][SV] Grenade GAS To=%s Amount=%.1f"),
							*GetNameSafe(Target),
							DamageAmount);
					}
				}
			}
		}

		// 폴백(ASC 없는 대상 대응)
		if (!bAppliedByGAS)
		{
			UGameplayStatics::ApplyDamage(Target, DamageAmount, InstigatorController.Get(), this, nullptr);

			UE_LOG(LogMosesCombat, Warning,
				TEXT("[DAMAGE][SV] Grenade Fallback To=%s Amount=%.1f"),
				*GetNameSafe(Target),
				DamageAmount);
		}

		++HitCount;
	}

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[GRENADE][SV] ApplyRadialDamage Radius=%.1f Hits=%d Any=%d"),
		Radius, HitCount, bAny ? 1 : 0);
}

void AMosesGrenadeProjectile::Multicast_PlayExplodeFX_Implementation(
	const FVector& Center,
	const FVector& HitNormal)
{
	// 파티클
	if (ExplosionVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ExplosionVFX,
			FTransform(HitNormal.Rotation(), Center));
	}

	// 사운드
	if (ExplosionSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			ExplosionSFX,
			Center);
	}
}

