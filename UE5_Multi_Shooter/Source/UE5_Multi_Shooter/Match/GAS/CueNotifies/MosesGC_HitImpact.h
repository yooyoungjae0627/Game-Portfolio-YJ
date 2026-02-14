#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "MosesGC_HitImpact.generated.h"

class UParticleSystem;
class USoundBase;
class UMosesWeaponData;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesGC_HitImpact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	// WeaponData가 없거나 에셋이 비어있을 때 폴백
	UPROPERTY(EditDefaultsOnly, Category="Cue|Fallback")
	TObjectPtr<UParticleSystem> FallbackImpactVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Cue|Fallback")
	TObjectPtr<USoundBase> FallbackImpactSFX = nullptr;

	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

private:
	static const UMosesWeaponData* GetWeaponDataFromParams(const FGameplayCueParameters& Parameters);
};
