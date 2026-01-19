#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode_FirstPerson.h"
#include "MosesCameraMode_Sniper2x.generated.h"

/**
 * UMosesCameraMode_Sniper2x
 *
 * [기능]
 * - 저격 2배율: 1인칭 기반 + FOV를 절반으로 줄여 확대감을 만든다.
 *
 * [명세]
 * - 예: 기본 80 → 40
 * - BlendTime을 조금 길게 하면 "조준 전환 느낌"이 더 좋다.
 */
UCLASS(Blueprintable)
class UE5_MULTI_SHOOTER_API UMosesCameraMode_Sniper2x : public UMosesCameraMode_FirstPerson
{
	GENERATED_BODY()

public:
	UMosesCameraMode_Sniper2x(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
