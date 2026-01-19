#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"

#include "GameFramework/Pawn.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"

// ---------------- FMosesCameraModeView ----------------

FMosesCameraModeView::FMosesCameraModeView()
	: Location(ForceInit)
	, Rotation(ForceInit)
	, ControlRotation(ForceInit)
	, FieldOfView(Moses_CAMERA_DEFAULT_FOV)
{
}

void FMosesCameraModeView::Blend(const FMosesCameraModeView& Other, float OtherWeight)
{
	if (OtherWeight <= 0.0f)
	{
		return;
	}

	if (OtherWeight >= 1.0f)
	{
		*this = Other;
		return;
	}

	// 위치/FOV는 Lerp, 회전은 Normalize 후 가중치 적용
	Location = FMath::Lerp(Location, Other.Location, OtherWeight);

	const FRotator DeltaRot = (Other.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + (OtherWeight * DeltaRot);

	const FRotator DeltaControlRot = (Other.ControlRotation - ControlRotation).GetNormalized();
	ControlRotation = ControlRotation + (OtherWeight * DeltaControlRot);

	FieldOfView = FMath::Lerp(FieldOfView, Other.FieldOfView, OtherWeight);
}

// ---------------- UMosesCameraMode ----------------

UMosesCameraMode::UMosesCameraMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 기본값은 헤더 초기값 사용
}

UMosesCameraComponent* UMosesCameraMode::GetMosesCameraComponent() const
{
	// 스택에서 NewObject 생성 시 Outer=CameraComponent로 생성됨
	return CastChecked<UMosesCameraComponent>(GetOuter());
}

AActor* UMosesCameraMode::GetTargetActor() const
{
	const UMosesCameraComponent* CamComp = GetMosesCameraComponent();
	return CamComp ? CamComp->GetTargetActor() : nullptr;
}

FVector UMosesCameraMode::GetPivotLocation() const
{
	const AActor* Target = GetTargetActor();
	check(Target);

	// Pawn이면 눈 위치(뷰 위치)를 pivot으로 사용
	if (const APawn* Pawn = Cast<APawn>(Target))
	{
		return Pawn->GetPawnViewLocation();
	}

	return Target->GetActorLocation();
}

FRotator UMosesCameraMode::GetPivotRotation() const
{
	const AActor* Target = GetTargetActor();
	check(Target);

	// Pawn이면 보통 ControlRotation 기반 시야 회전
	if (const APawn* Pawn = Cast<APawn>(Target))
	{
		return Pawn->GetViewRotation();
	}

	return Target->GetActorRotation();
}

void UMosesCameraMode::ResetForNewPush(float InitialBlendWeight)
{
	// 스택 Top에 올라올 때 블렌딩을 새로 시작
	BlendAlpha = 0.0f;
	BlendWeight = FMath::Clamp(InitialBlendWeight, 0.0f, 1.0f);
}

void UMosesCameraMode::UpdateCameraMode(float DeltaTime)
{
	// 1) 이 모드 기준 View 계산
	UpdateView(DeltaTime);

	// 2) 이 모드 BlendWeight 업데이트
	UpdateBlending(DeltaTime);
}

void UMosesCameraMode::UpdateView(float DeltaTime)
{
	// 기본 구현: Pivot 그대로
	const FVector PivotLoc = GetPivotLocation();
	FRotator PivotRot = GetPivotRotation();

	PivotRot.Pitch = FMath::ClampAngle(PivotRot.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLoc;
	View.Rotation = PivotRot;
	View.ControlRotation = PivotRot;
	View.FieldOfView = FieldOfView;
}

void UMosesCameraMode::UpdateBlending(float DeltaTime)
{
	if (BlendTime > 0.0f)
	{
		BlendAlpha = FMath::Clamp(BlendAlpha + (DeltaTime / BlendTime), 0.0f, 1.0f);
	}
	else
	{
		BlendAlpha = 1.0f;
	}

	const float Expo = (BlendExponent > 0.0f) ? BlendExponent : 1.0f;

	switch (BlendFunction)
	{
	case EMosesCameraModeBlendFunction::Linear:
		BlendWeight = BlendAlpha;
		break;
	case EMosesCameraModeBlendFunction::EaseIn:
		BlendWeight = FMath::InterpEaseIn(0.0f, 1.0f, BlendAlpha, Expo);
		break;
	case EMosesCameraModeBlendFunction::EaseOut:
		BlendWeight = FMath::InterpEaseOut(0.0f, 1.0f, BlendAlpha, Expo);
		break;
	case EMosesCameraModeBlendFunction::EaseInOut:
		BlendWeight = FMath::InterpEaseInOut(0.0f, 1.0f, BlendAlpha, Expo);
		break;
	default:
		BlendWeight = BlendAlpha;
		break;
	}
}

// ---------------- UMosesCameraModeStack ----------------

UMosesCameraModeStack::UMosesCameraModeStack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMosesCameraMode* UMosesCameraModeStack::GetCameraModeInstance(TSubclassOf<UMosesCameraMode> CameraModeClass)
{
	check(CameraModeClass);

	for (UMosesCameraMode* Mode : CameraModeInstances)
	{
		if (Mode && Mode->GetClass() == CameraModeClass)
		{
			return Mode;
		}
	}

	UMosesCameraMode* NewMode = NewObject<UMosesCameraMode>(GetOuter(), CameraModeClass);
	check(NewMode);

	CameraModeInstances.Add(NewMode);
	return NewMode;
}

void UMosesCameraModeStack::PushCameraMode(TSubclassOf<UMosesCameraMode> CameraModeClass)
{
	if (!CameraModeClass)
	{
		return;
	}

	UMosesCameraMode* Mode = GetCameraModeInstance(CameraModeClass);
	check(Mode);

	// 이미 Top이면 유지
	if (CameraModeStack.Num() > 0 && CameraModeStack[0] == Mode)
	{
		return;
	}

	// 스택 중간에 있으면 제거 후 Top으로 이동
	const int32 ExistingIndex = CameraModeStack.Find(Mode);
	if (ExistingIndex != INDEX_NONE)
	{
		CameraModeStack.RemoveAt(ExistingIndex);
	}

	// 블렌딩 시작값 결정: 다른 모드가 있으면 0부터 시작(부드러운 전환)
	const float InitialWeight = (CameraModeStack.Num() > 0 && Mode->BlendTime > 0.0f) ? 0.0f : 1.0f;
	Mode->ResetForNewPush(InitialWeight);

	CameraModeStack.Insert(Mode, 0);

	// Bottom은 항상 1.0
	if (CameraModeStack.Num() > 0)
	{
		CameraModeStack.Last()->BlendWeight = 1.0f;
		CameraModeStack.Last()->BlendAlpha = 1.0f;
	}
}

void UMosesCameraModeStack::UpdateStack(float DeltaTime)
{
	for (int32 i = 0; i < CameraModeStack.Num(); ++i)
	{
		UMosesCameraMode* Mode = CameraModeStack[i];
		check(Mode);

		Mode->UpdateCameraMode(DeltaTime);

		// Top부터 내려가며, 어떤 모드가 1.0이면 그 아래는 제거 가능
		if (Mode->BlendWeight >= 1.0f)
		{
			const int32 RemoveIndex = i + 1;
			if (RemoveIndex < CameraModeStack.Num())
			{
				CameraModeStack.RemoveAt(RemoveIndex, CameraModeStack.Num() - RemoveIndex);
			}
			break;
		}
	}

	// Bottom은 항상 1.0 유지
	if (CameraModeStack.Num() > 0)
	{
		CameraModeStack.Last()->BlendWeight = 1.0f;
		CameraModeStack.Last()->BlendAlpha = 1.0f;
	}
}

void UMosesCameraModeStack::BlendStack(FMosesCameraModeView& OutCameraModeView) const
{
	if (CameraModeStack.Num() <= 0)
	{
		return;
	}

	const int32 LastIndex = CameraModeStack.Num() - 1;
	const UMosesCameraMode* Bottom = CameraModeStack[LastIndex];
	check(Bottom);

	OutCameraModeView = Bottom->View;

	// Bottom → Top 방향으로 Blend
	for (int32 i = LastIndex - 1; i >= 0; --i)
	{
		const UMosesCameraMode* Mode = CameraModeStack[i];
		check(Mode);

		OutCameraModeView.Blend(Mode->View, Mode->BlendWeight);
	}
}

void UMosesCameraModeStack::EvaluateStack(float DeltaTime, FMosesCameraModeView& OutCameraModeView)
{
	UpdateStack(DeltaTime);
	BlendStack(OutCameraModeView);
}
