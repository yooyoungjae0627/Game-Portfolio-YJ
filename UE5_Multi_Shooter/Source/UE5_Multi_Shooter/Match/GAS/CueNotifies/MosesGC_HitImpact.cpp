#include "UE5_Multi_Shooter/Match/GAS/CueNotifies/MosesGC_HitImpact.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"

#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

const UMosesWeaponData* UMosesGC_HitImpact::GetWeaponDataFromParams(const FGameplayCueParameters& Parameters)
{
	return Cast<UMosesWeaponData>(Parameters.SourceObject.Get());
}

bool UMosesGC_HitImpact::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!Target)
	{
		return false;
	}

	const FHitResult* HR = Parameters.EffectContext.GetHitResult();
	if (!HR)
	{
		return false;
	}

	const UMosesWeaponData* WD = GetWeaponDataFromParams(Parameters);

	UParticleSystem* ImpactVFX = nullptr;
	USoundBase* ImpactSFX = nullptr;

	if (WD)
	{
		ImpactVFX = WD->ImpactVFX.LoadSynchronous();
		ImpactSFX = WD->ImpactSFX.LoadSynchronous();
	}

	if (!ImpactVFX) ImpactVFX = FallbackImpactVFX;
	if (!ImpactSFX) ImpactSFX = FallbackImpactSFX;

	const FVector Loc = HR->ImpactPoint;
	const FRotator Rot = HR->ImpactNormal.Rotation();

	if (ImpactVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(Target, ImpactVFX, Loc, Rot, true);
	}

	if (ImpactSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(Target, ImpactSFX, Loc);
	}

	return true;
}
