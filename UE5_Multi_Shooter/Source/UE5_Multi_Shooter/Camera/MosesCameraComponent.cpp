#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"

UMosesCameraComponent::UMosesCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesCameraComponent::OnRegister()
{
	Super::OnRegister();

	// 스택 객체는 BeginPlay 없이도 되므로 NewObject로 생성
	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UMosesCameraModeStack>(this);
	}
}

void UMosesCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	// 1) 이번 프레임에 사용할 모드를 스택에 Push
	UpdateCameraModes();

	// 2) 스택 평가(업데이트+블렌딩)로 최종 View 계산
	FMosesCameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime, CameraModeView);

	// 3) 로컬 컨트롤러만 ControlRotation 갱신(서버/타인 영향 차단)
	UpdateLocalControlRotationIfNeeded(CameraModeView);

	// 4) 카메라 컴포넌트 트랜스폼과 DesiredView에 적용
	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	ApplyCameraViewToDesiredView(CameraModeView, DesiredView);
}

void UMosesCameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	const TSubclassOf<UMosesCameraMode> ModeClass = ResolveCameraModeClass();
	if (ModeClass)
	{
		CameraModeStack->PushCameraMode(ModeClass);
		return;
	}

	if (!bLoggedNoModeClassOnce)
	{
		UE_LOG(LogMosesCamera, Error, TEXT("[MosesCamera] No CameraModeClass (Delegate + Default are null)"));
		bLoggedNoModeClassOnce = true;
	}
}

TSubclassOf<UMosesCameraMode> UMosesCameraComponent::ResolveCameraModeClass() const
{
	// Delegate가 있으면 우선 사용
	if (DetermineCameraModeDelegate.IsBound())
	{
		return DetermineCameraModeDelegate.Execute();
	}

	// 없으면 폴백
	if (!bLoggedNoDelegateOnce)
	{
		UE_LOG(LogMosesCamera, Warning, TEXT("[MosesCamera] Delegate not bound -> fallback DefaultCameraModeClass"));
		bLoggedNoDelegateOnce = true;
	}

	return DefaultCameraModeClass;
}

void UMosesCameraComponent::UpdateLocalControlRotationIfNeeded(const FMosesCameraModeView& CameraModeView)
{
	if (APawn* Pawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = Pawn->GetController<APlayerController>())
		{
			// 로컬 컨트롤러만 갱신
			if (PC->IsLocalController())
			{
				PC->SetControlRotation(CameraModeView.ControlRotation);
			}
		}
	}
}

void UMosesCameraComponent::ApplyCameraViewToDesiredView(const FMosesCameraModeView& CameraModeView, FMinimalViewInfo& DesiredView)
{
	DesiredView.Location = CameraModeView.Location;
	DesiredView.Rotation = CameraModeView.Rotation;
	DesiredView.FOV = CameraModeView.FieldOfView;

	// CameraComponent 기본 설정을 그대로 전달
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
