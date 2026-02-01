#include "UE5_Multi_Shooter/Combat/MosesGrenadeProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Engine/World.h"

// ✅ [FINAL FIX] FOverlapResult “정의”를 확실히 포함
// - 일부 UE5 구성/인클루드 경로에서는 WorldCollision.h가 전방선언만 노출되는 케이스가 있음.
// - 아래 2개를 같이 넣으면 FOverlapResult의 정의가 반드시 들어온다.
#include "Engine/OverlapResult.h"
#include "Engine/EngineTypes.h"

#include "Kismet/GameplayStatics.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/Weapon/MosesWeaponData.h"


AMosesGrenadeProjectile::AMosesGrenadeProjectile()
{
	bReplicates = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);

	Collision->InitSphereRadius(12.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Overlap);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.35f;
	Movement->InitialSpeed = 2400.0f;
	Movement->MaxSpeed = 3200.0f;
}

void AMosesGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (Collision)
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnCollisionBeginOverlap);
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
}

void AMosesGrenadeProjectile::Launch_Server(const FVector& Dir, float Speed)
{
	check(HasAuthority());

	if (Movement)
	{
		Movement->Velocity = Dir.GetSafeNormal() * FMath::Max(1.0f, Speed);
	}
}

void AMosesGrenadeProjectile::OnCollisionBeginOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bExploded)
	{
		return;
	}

	Explode_Server(GetActorLocation());
}

void AMosesGrenadeProjectile::Explode_Server(const FVector& ExplodeLocation)
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}

	bExploded = true;

	ApplyRadialDamage_Server(ExplodeLocation);

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[GRENADE][SV] Explode Radius=%.1f Damage=%.1f Weapon=%s"),
		Radius,
		DamageAmount,
		WeaponData.IsValid() ? *WeaponData->WeaponId.ToString() : TEXT("None"));

	Multicast_PlayExplodeFX(ExplodeLocation);

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

	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	const bool bAny = World->OverlapMultiByChannel(Results, Center, FQuat::Identity, ECC_Pawn, Sphere, Params);

	int32 HitCount = 0;

	for (const FOverlapResult& R : Results)
	{
		AActor* Target = R.GetActor(); // ✅ 이제 GetActor() 가능
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
					Ctx.AddInstigator(DamageCauser.Get(), InstigatorController.Get());
					Ctx.AddSourceObject(this);
					if (WeaponData.IsValid())
					{
						Ctx.AddSourceObject(WeaponData.Get());
					}

					const FGameplayEffectSpecHandle SpecHandle = SrcASC->MakeOutgoingSpec(DamageGE, 1.f, Ctx);
					if (SpecHandle.IsValid())
					{
						const float SignedDamage = -FMath::Abs(DamageAmount);
						SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Damage, SignedDamage);
						SrcASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

						bAppliedByGAS = true;

						UE_LOG(LogMosesGAS, Warning,
							TEXT("[DAMAGE][SV] Applied To=%s Amount=%.1f (Grenade GAS)"),
							*GetNameSafe(Target),
							DamageAmount);
					}
				}
			}
		}

		// 폴백(ASC 없는 대상 대응)
		if (!bAppliedByGAS)
		{
			UGameplayStatics::ApplyDamage(Target, DamageAmount, InstigatorController.Get(), DamageCauser.Get(), nullptr);

			UE_LOG(LogMosesCombat, Warning,
				TEXT("[DAMAGE][SV] Applied To=%s Amount=%.1f (Grenade Fallback)"),
				*GetNameSafe(Target),
				DamageAmount);
		}

		++HitCount;
	}

	UE_LOG(LogMosesCombat, Warning, TEXT("[GRENADE][SV] Explode Radius=%.1f Hits=%d Any=%d"), Radius, HitCount, bAny ? 1 : 0);
}

void AMosesGrenadeProjectile::Multicast_PlayExplodeFX_Implementation(const FVector& Center)
{
	// 코스메틱 FX/SFX는 BP/에셋에서 처리(여기는 로그만)
	UE_LOG(LogMosesCombat, Verbose, TEXT("[GRENADE][CL] ExplodeFX Center=%s"), *Center.ToCompactString());
}
