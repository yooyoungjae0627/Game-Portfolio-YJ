// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesCameraComponent.h"

#include "MosesCameraMode.h"


UMosesCameraComponent::UMosesCameraComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), CameraModeStack(nullptr)
{

}

void UMosesCameraComponent::OnRegister()
{
	Super::OnRegister();

	if (CameraModeStack == nullptr)
	{
		// 초기화 (Beingplay와 같은)가 딱히 필요없는 객체로 NewObject로 할당
		CameraModeStack = NewObject<UMosesCameraModeStack>(this);
	}
}

void UMosesCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	UpdateCameraModes();

	FMosesCameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime, CameraModeView);

	if (APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
		{
			// ✅ 로컬 컨트롤러만 ControlRotation을 직접 갱신 (서버/타인 영향 차단)
			if (PC->IsLocalController())
			{
				PC->SetControlRotation(CameraModeView.ControlRotation);
			}
		}
	}

	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	DesiredView.Location = CameraModeView.Location;
	DesiredView.Rotation = CameraModeView.Rotation;
	DesiredView.FOV = CameraModeView.FieldOfView;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}

void UMosesCameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	TSubclassOf<UMosesCameraMode> CameraModeClass = nullptr;

	// 1) 정상 루트: HeroComponent가 바인딩한 Delegate에서 가져오기
	if (DetermineCameraModeDelegate.IsBound())
	{
		CameraModeClass = DetermineCameraModeDelegate.Execute();
	}
	else
	{
		// 2) 폴백 루트: DefaultCameraModeClass 사용 (즉시 화면 정상화)
		static bool bLoggedOnce_NoDelegate = false;
		if (!bLoggedOnce_NoDelegate)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MosesCamera] DetermineCameraModeDelegate NOT bound -> use DefaultCameraModeClass fallback"));
			bLoggedOnce_NoDelegate = true;
		}

		CameraModeClass = DefaultCameraModeClass;
	}

	if (CameraModeClass)
	{
		CameraModeStack->PushCameraMode(CameraModeClass);
		UE_LOG(LogTemp, Verbose, TEXT("[MosesCamera] PushCameraMode=%s"), *GetNameSafe(CameraModeClass));
	}
	else
	{
		static bool bLoggedOnce_Null = false;
		if (!bLoggedOnce_Null)
		{
			UE_LOG(LogTemp, Error, TEXT("[MosesCamera] No CameraModeClass (delegate + fallback both null). Camera will be invalid."));
			bLoggedOnce_Null = true;
		}
	}
}

