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
	if (!ModeClass)
	{
		if (!bLoggedNoModeClassOnce)
		{
			UE_LOG(LogMosesCamera, Error,
				TEXT("[MosesCamera] No CameraModeClass (Delegate + Default are null). Force fallback needed."));
			bLoggedNoModeClassOnce = true;
		}

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

	// --------------------------------------------------------------------
	// [DAY8][MOD] Scope Local Override (연출 전용)
	// - 기존 카메라 시스템을 깨지 않기 위해 "최종 DesiredView 단계"에서만 최소 삽입한다.
	// --------------------------------------------------------------------
	if (bScopeActive_Local)
	{
		DesiredView.FOV = ScopedFOV_Local;

		// PostProcess가 0이면 설정이 전달되지 않을 수 있으므로, 최소 1.0으로 올려준다.
		DesiredView.PostProcessBlendWeight = FMath::Max(DesiredView.PostProcessBlendWeight, 1.0f);

		// "이동 블러" 체감: MotionBlurAmount 사용(프로젝트에서 다른 PP 사용 시 교체 가능)
		DesiredView.PostProcessSettings.bOverride_MotionBlurAmount = true;
		DesiredView.PostProcessSettings.MotionBlurAmount = ScopeBlurStrength_Local;
	}
}

// --------------------------------------------------------------------
// [DAY8] Scope API
// --------------------------------------------------------------------

void UMosesCameraComponent::SetSniperScopeActive_Local(bool bActive, float InScopedFOV)
{
	bScopeActive_Local = bActive;
	ScopedFOV_Local = FMath::Clamp(InScopedFOV, 5.0f, 170.0f);
}

void UMosesCameraComponent::SetScopeBlurStrength_Local(float InStrength01)
{
	ScopeBlurStrength_Local = FMath::Clamp(InStrength01, 0.0f, 1.0f);
}
