#include "UE5_Multi_Shooter/Match/GAS/CueNotifies/MosesGC_MuzzleFlash.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

const UMosesWeaponData* UMosesGC_MuzzleFlash::GetWeaponDataFromParams(const FGameplayCueParameters& Parameters)
{
	return Cast<UMosesWeaponData>(Parameters.SourceObject.Get());
}

USkeletalMeshComponent* UMosesGC_MuzzleFlash::FindBestMeshWithSocket(AActor* Target, const FName SocketName)
{
	if (!Target)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Target->GetComponents<USkeletalMeshComponent>(Meshes);

	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->DoesSocketExist(SocketName))
		{
			return Mesh;
		}
	}

	// 폴백: 소켓 없는 경우 첫 번째 메쉬라도 반환(월드 스폰 폴백을 위해)
	return (Meshes.Num() > 0) ? Meshes[0] : nullptr;
}

bool UMosesGC_MuzzleFlash::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!Target)
	{
		return false;
	}

	const UMosesWeaponData* WD = GetWeaponDataFromParams(Parameters);

	UParticleSystem* MuzzleVFX = nullptr;
	USoundBase* MuzzleSFX = nullptr;
	FName SocketName(TEXT("MuzzleFlash"));

	if (WD)
	{
		MuzzleVFX = WD->MuzzleVFX.LoadSynchronous();
		MuzzleSFX = WD->FireSFX.LoadSynchronous();
		if (!WD->MuzzleSocketName.IsNone())
		{
			SocketName = WD->MuzzleSocketName;
		}
	}

	if (!MuzzleVFX) MuzzleVFX = FallbackMuzzleVFX;
	if (!MuzzleSFX) MuzzleSFX = FallbackMuzzleSFX;

	USkeletalMeshComponent* Mesh = FindBestMeshWithSocket(Target, SocketName);
	const bool bHasSocket = (Mesh && Mesh->DoesSocketExist(SocketName));

	const FVector Loc = bHasSocket ? Mesh->GetSocketLocation(SocketName) : Target->GetActorLocation();
	const FRotator Rot = bHasSocket ? Mesh->GetSocketRotation(SocketName) : Target->GetActorRotation();

	if (MuzzleVFX)
	{
		if (bHasSocket)
		{
			UGameplayStatics::SpawnEmitterAttached(
				MuzzleVFX,
				Mesh,
				SocketName,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true);
		}
		else
		{
			UGameplayStatics::SpawnEmitterAtLocation(Target, MuzzleVFX, Loc, Rot, true);
		}
	}

	if (MuzzleSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(Target, MuzzleSFX, Loc);
	}

	return true;
}
