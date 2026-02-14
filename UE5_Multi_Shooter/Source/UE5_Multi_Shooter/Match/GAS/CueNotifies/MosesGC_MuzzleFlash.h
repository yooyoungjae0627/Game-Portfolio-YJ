#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "MosesGC_MuzzleFlash.generated.h"

class UParticleSystem;
class USoundBase;
class UMosesWeaponData;
class USkeletalMeshComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesGC_MuzzleFlash : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	// WeaponData가 없거나 에셋이 비어있을 때 폴백 (비워도 됨)
	UPROPERTY(EditDefaultsOnly, Category="Cue|Fallback")
	TObjectPtr<UParticleSystem> FallbackMuzzleVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Cue|Fallback")
	TObjectPtr<USoundBase> FallbackMuzzleSFX = nullptr;

	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

private:
	static const UMosesWeaponData* GetWeaponDataFromParams(const FGameplayCueParameters& Parameters);
	static USkeletalMeshComponent* FindBestMeshWithSocket(AActor* Target, const FName SocketName);
};
