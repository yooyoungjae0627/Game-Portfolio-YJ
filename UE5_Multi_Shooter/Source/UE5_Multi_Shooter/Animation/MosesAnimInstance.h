#pragma once

#include "Animation/AnimInstance.h"
#include "MosesAnimInstance.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMosesAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---------------------------
	// UAnimInstance
	// ---------------------------
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

public:
	// ---------------------------
	// Locomotion (AnimBP reads)
	// ---------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsSprinting = false;

	// 기존 필드 유지
	UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
	float GroundDistance = -1.0f;

private:
	TWeakObjectPtr<class APawn> CachedPawn;
};
