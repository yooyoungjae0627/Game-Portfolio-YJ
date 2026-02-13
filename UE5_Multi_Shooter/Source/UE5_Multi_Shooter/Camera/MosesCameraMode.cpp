// ============================================================================
// UE5_Multi_Shooter/Camera/MosesCameraMode.cpp  (FULL - CLEAN)
// ============================================================================

#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"

#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/Pawn.h"

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
}

UMosesCameraComponent* UMosesCameraMode::GetMosesCameraComponent() const
{
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

	if (const APawn* Pawn = Cast<APawn>(Target))
	{
		return Pawn->GetViewRotation();
	}

	return Target->GetActorRotation();
}

void UMosesCameraMode::ResetForNewPush(float InitialBlendWeight)
{
	BlendAlpha = 0.0f;
	BlendWeight = FMath::Clamp(InitialBlendWeight, 0.0f, 1.0f);
}

void UMosesCameraMode::UpdateCameraMode(float DeltaTime)
{
	UpdateView(DeltaTime);
	UpdateBlending(DeltaTime);
}

void UMosesCameraMode::UpdateView(float DeltaTime)
{
	const FVector PivotLoc = GetPivotLocation();
	const FRotator PivotRotRaw = GetPivotRotation();
	const FRotator PivotRot = ClampPivotRotationPitch_Safe(PivotRotRaw);

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

void UMosesCameraMode::GetEffectivePitchLimits(float& OutMin, float& OutMax) const
{
	const float ProjectMin = -89.0f;
	const float ProjectMax = 89.0f;

	OutMin = FMath::Min(ViewPitchMin, ProjectMin);
	OutMax = FMath::Max(ViewPitchMax, ProjectMax);

	if (OutMin > OutMax)
	{
		OutMin = ProjectMin;
		OutMax = ProjectMax;
	}
}

FRotator UMosesCameraMode::ClampPivotRotationPitch_Safe(const FRotator& InPivotRot) const
{
	float EffectiveMin = 0.0f;
	float EffectiveMax = 0.0f;
	GetEffectivePitchLimits(EffectiveMin, EffectiveMax);

	FRotator Out = InPivotRot;
	const float RawPitch = Out.Pitch;

	Out.Pitch = FMath::ClampAngle(Out.Pitch, EffectiveMin, EffectiveMax);

	if (!bLoggedPitchClampOnce && !FMath::IsNearlyEqual(RawPitch, Out.Pitch, 0.01f))
	{
		bLoggedPitchClampOnce = true;

		UE_LOG(LogMosesCamera, Warning,
			TEXT("[PITCH][CLAMP][CAMMODE] Mode=%s Raw=%.2f Clamped=%.2f Min=%.2f Max=%.2f"),
			*GetNameSafe(GetClass()),
			RawPitch,
			Out.Pitch,
			EffectiveMin,
			EffectiveMax);
	}

	return Out;
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

	if (CameraModeStack.Num() > 0 && CameraModeStack[0] == Mode)
	{
		return;
	}

	const int32 ExistingIndex = CameraModeStack.Find(Mode);
	if (ExistingIndex != INDEX_NONE)
	{
		CameraModeStack.RemoveAt(ExistingIndex);
	}

	const float InitialWeight = (CameraModeStack.Num() > 0 && Mode->BlendTime > 0.0f) ? 0.0f : 1.0f;
	Mode->ResetForNewPush(InitialWeight);

	CameraModeStack.Insert(Mode, 0);

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
