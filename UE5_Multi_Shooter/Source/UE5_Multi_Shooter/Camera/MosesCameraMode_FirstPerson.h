#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "MosesCameraMode_FirstPerson.generated.h"

/**
 * UMosesCameraMode_FirstPerson
 *
 * [기능]
 * - 1인칭: Pivot(눈 위치)에 카메라를 거의 그대로 둔다.
 *
 * [명세]
 * - DefaultEyeOffset으로 아주 약간의 시점 조정 가능.
 */
UCLASS(Blueprintable)
class UE5_MULTI_SHOOTER_API UMosesCameraMode_FirstPerson : public UMosesCameraMode
{
	GENERATED_BODY()

public:
	UMosesCameraMode_FirstPerson(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void UpdateView(float DeltaTime) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|FirstPerson")
	FVector DefaultEyeOffset = FVector::ZeroVector;
};
