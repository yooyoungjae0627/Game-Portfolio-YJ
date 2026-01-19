#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "MosesCameraMode_ThirdPerson.generated.h"

class UCurveVector;

/**
 * UMosesCameraMode_ThirdPerson
 *
 * [기능]
 * - 3인칭: Pivot 기준으로 뒤/위 오프셋을 적용해 카메라를 배치한다.
 *
 * [명세]
 * - TargetOffsetCurve가 있으면 Pitch에 따라 오프셋을 커브로 변경 가능.
 * - 없으면 DefaultTargetOffset 사용.
 */
UCLASS(Blueprintable)
class UE5_MULTI_SHOOTER_API UMosesCameraMode_ThirdPerson : public UMosesCameraMode
{
	GENERATED_BODY()

public:
	UMosesCameraMode_ThirdPerson(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void UpdateView(float DeltaTime) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|ThirdPerson")
	TObjectPtr<const UCurveVector> TargetOffsetCurve = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|ThirdPerson")
	FVector DefaultTargetOffset = FVector(-300.0f, 0.0f, 75.0f);
};
