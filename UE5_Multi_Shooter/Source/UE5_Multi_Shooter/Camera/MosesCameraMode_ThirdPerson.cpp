// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesCameraMode_ThirdPerson.h"
#include "Curves/CurveVector.h"


UMosesCameraMode_ThirdPerson::UMosesCameraMode_ThirdPerson(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UMosesCameraMode_ThirdPerson::UpdateView(float DeltaTime)
{
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;

	FVector TargetOffset = DefaultTargetOffset;

	if (TargetOffsetCurve)
	{
		TargetOffset = TargetOffsetCurve->GetVectorValue(PivotRotation.Pitch);
	}

	View.Location = PivotLocation + PivotRotation.RotateVector(TargetOffset);
}

