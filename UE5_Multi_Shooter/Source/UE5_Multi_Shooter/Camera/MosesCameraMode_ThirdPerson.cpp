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

    static bool bLoggedOnce = false;

    // TargetOffsetCurve가 오버라이드되어 있다면, Curve에 값을 가져와서 적용 진행
    // - Camera 관점에서 Charater의 어느 부분에 Target으로 할지 결정하는 것으로 이해하면 됨
    if (TargetOffsetCurve)
    {
        const FVector TargetOffset = TargetOffsetCurve->GetVectorValue(PivotRotation.Pitch);
        View.Location = PivotLocation + PivotRotation.RotateVector(TargetOffset);

        if (!bLoggedOnce)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[MosesCamera] ThirdPerson: Has TargetOffsetCurve. Pivot=%s, Offset=%s, FinalLocation=%s"),
                *PivotLocation.ToString(),
                *TargetOffset.ToString(),
                *View.Location.ToString());
            bLoggedOnce = true;
        }
    }
    else
    {
        if (!bLoggedOnce)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[MosesCamera] ThirdPerson: NO TargetOffsetCurve! Using pure PivotLocation as camera (will look like FPS). Pivot=%s"),
                *PivotLocation.ToString());
            bLoggedOnce = true;
        }
    }
}


