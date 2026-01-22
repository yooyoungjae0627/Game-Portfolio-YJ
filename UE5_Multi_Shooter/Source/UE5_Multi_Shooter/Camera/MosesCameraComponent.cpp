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

	TSubclassOf<UMosesCameraMode> ModeClass = ResolveCameraModeClass();

	// 어떤 이유로든 모드가 없다면, "안전 폴백"을 강제한다.
	// - DefaultCameraModeClass가 BP에서 세팅되지 않았거나
	// - Delegate 바인딩이 실패한 타이밍 등에서 카메라가 원점/이상한 값으로 튀는 것을 차단
	if (!ModeClass)
	{
		if (!bLoggedNoModeClassOnce)
		{
			UE_LOG(LogMosesCamera, Error,
				TEXT("[MosesCamera] No CameraModeClass (Delegate + Default are null). Force fallback needed."));
			bLoggedNoModeClassOnce = true;
		}

		// [IMPORTANT]
		// 여기서 바로 특정 클래스(ThirdPerson)를 하드코딩하고 싶으면
		// 프로젝트 정책상 "에셋 의존"이 생길 수 있으니, 최소한 DefaultCameraModeClass는 반드시 세팅하도록 하고
		// 이 강제 폴백은 "개발 중 안전장치"로만 둔다.
		ModeClass = DefaultCameraModeClass;
	}

	if (ModeClass)
	{
		CameraModeStack->PushCameraMode(ModeClass);
	}
}

TSubclassOf<UMosesCameraMode> UMosesCameraComponent::ResolveCameraModeClass() const
{
	if (DetermineCameraModeDelegate.IsBound())
	{
		return DetermineCameraModeDelegate.Execute();
	}

	if (!bLoggedNoDelegateOnce)
	{
		UE_LOG(LogMosesCamera, Warning,
			TEXT("[MosesCamera] Delegate not bound -> fallback DefaultCameraModeClass=%s"),
			*GetNameSafe(DefaultCameraModeClass));
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
