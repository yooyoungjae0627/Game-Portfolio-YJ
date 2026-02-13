// ============================================================================
// UE5_Multi_Shooter/Camera/MosesCameraComponent.cpp  (FULL - CLEAN)
// ============================================================================

#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UMosesCameraComponent::UMosesCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UMosesCameraComponent* UMosesCameraComponent::FindCameraComponent(const AActor* Actor)
{
	return (Actor ? Actor->FindComponentByClass<UMosesCameraComponent>() : nullptr);
}

AActor* UMosesCameraComponent::GetTargetActor() const
{
	return GetOwner();
}

void UMosesCameraComponent::OnRegister()
{
	Super::OnRegister();

	// 스택 객체는 BeginPlay 없이도 사용 가능하므로 Register 시점에 생성한다.
	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UMosesCameraModeStack>(this);
	}
}

void UMosesCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	// 1) 이번 프레임 모드 결정 → 스택 Push
	UpdateCameraModes();

	// 2) 스택 Evaluate(업데이트+블렌딩) → 최종 View 계산
	FMosesCameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime, CameraModeView);

	// 3) 로컬 컨트롤러만 ControlRotation 갱신
	UpdateLocalControlRotationIfNeeded(CameraModeView);

	// 4) 컴포넌트 트랜스폼 및 DesiredView 반영
	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	ApplyCameraViewToDesiredView(CameraModeView, DesiredView);
}

void UMosesCameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	TSubclassOf<UMosesCameraMode> ModeClass = ResolveCameraModeClass();

	// 어떤 이유로든 모드가 없다면 "안전 폴백"을 시도한다.
	if (!ModeClass)
	{
		if (!bLoggedNoModeClassOnce)
		{
			UE_LOG(LogMosesCamera, Error,
				TEXT("[MosesCamera] No CameraModeClass (Delegate + Default are null)."));
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

	// CameraComponent 기본 설정 전달
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

	// 스코프(로컬 연출): 최종 DesiredView 단계에서만 최소 오버라이드
	if (bScopeActive_Local)
	{
		DesiredView.FOV = ScopedFOV_Local;

		// PostProcess가 0이면 설정이 전달되지 않을 수 있으므로 최소 1.0 보장
		DesiredView.PostProcessBlendWeight = FMath::Max(DesiredView.PostProcessBlendWeight, 1.0f);

		DesiredView.PostProcessSettings.bOverride_MotionBlurAmount = true;
		DesiredView.PostProcessSettings.MotionBlurAmount = ScopeBlurStrength_Local;
	}
}

void UMosesCameraComponent::SetSniperScopeActive_Local(bool bActive, float InScopedFOV)
{
	bScopeActive_Local = bActive;
	ScopedFOV_Local = FMath::Clamp(InScopedFOV, 5.0f, 170.0f);
}

void UMosesCameraComponent::SetScopeBlurStrength_Local(float InStrength01)
{
	ScopeBlurStrength_Local = FMath::Clamp(InStrength01, 0.0f, 1.0f);
}

bool UMosesCameraComponent::IsSniperScopeActive_Local() const
{
	return bScopeActive_Local;
}
