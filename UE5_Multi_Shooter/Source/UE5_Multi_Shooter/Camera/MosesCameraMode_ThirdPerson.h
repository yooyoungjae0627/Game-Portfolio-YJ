// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MosesCameraMode.h"
#include "MosesCameraMode_ThirdPerson.generated.h"

class UCurveVector;

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class UE5_MULTI_SHOOTER_API UMosesCameraMode_ThirdPerson : public UMosesCameraMode
{
	GENERATED_BODY()
	
public:
	UMosesCameraMode_ThirdPerson(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void UpdateView(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	TObjectPtr<const UCurveVector> TargetOffsetCurve;
	
	
};
