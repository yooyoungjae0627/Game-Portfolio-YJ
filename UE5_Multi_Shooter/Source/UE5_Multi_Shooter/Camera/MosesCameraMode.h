#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MosesCameraMode.generated.h"

class UMosesCameraComponent;
template <class TClass> class TSubclassOf;

UENUM(BlueprintType)
enum class EMosesCameraModeBlendFunction : uint8
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut,
	COUNT
};

/**
 * CameraMode View Bundle
 * - Location/Rotation/ControlRotation/FOV
 */
struct FMosesCameraModeView
{
	FMosesCameraModeView();

	void Blend(const FMosesCameraModeView& Other, float OtherWeight);

	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};

/**
 * UMosesCameraMode
 *
 * - Camera calculation unit (ThirdPerson / FirstPerson / Sniper2x)
 * - Created/owned by UMosesCameraComponent (stack-managed)
 *
 * [FIX][PITCH]
 * - 일부 BP(CameraMode BP)에서 ViewPitchMin/Max가 잘못 좁게 설정되면
 *   "마우스 위/아래를 움직여도 화면이 Pitch로 회전하지 않는" 현상이 발생한다.
 * - 따라서 모드 값이 잘못되어도 프로젝트 기본 Pitch 범위는 "최소 보장"한다.
 */
UCLASS(Abstract, Blueprintable)
class UE5_MULTI_SHOOTER_API UMosesCameraMode : public UObject
{
	GENERATED_BODY()

public:
	UMosesCameraMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UMosesCameraComponent* GetMosesCameraComponent() const;
	AActor* GetTargetActor() const;

	FVector GetPivotLocation() const;
	FRotator GetPivotRotation() const;

	void UpdateCameraMode(float DeltaTime);
	void ResetForNewPush(float InitialBlendWeight);

protected:
	virtual void UpdateView(float DeltaTime);

	// =========================================================
	// [MOD][FIX] Pitch Safety
	// =========================================================
	void GetEffectivePitchLimits(float& OutMin, float& OutMax) const;
	FRotator ClampPivotRotationPitch_Safe(const FRotator& InPivotRot) const;

private:
	void UpdateBlending(float DeltaTime);

public:
	FMosesCameraModeView View;

	// -------------------- View Tunables --------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|View", meta = (UIMin = "5.0", UIMax = "170.0", ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 80.0f;

	/**
	 * BP에서 실수로 좁게 잡혀도 코드가 최소 보장(-89~+89)을 적용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|View", meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMin = -89.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|View", meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMax = 89.0f;

	// -------------------- Blending --------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Blending")
	float BlendTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Blending")
	float BlendExponent = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Blending")
	EMosesCameraModeBlendFunction BlendFunction = EMosesCameraModeBlendFunction::EaseOut;

public:
	float BlendAlpha = 1.0f;
	float BlendWeight = 1.0f;

private:
	// [MOD] Clamp 발생 시 1회만 로그(스팸 방지)
	mutable bool bLoggedPitchClampOnce = false;
};

/**
 * UMosesCameraModeStack
 * - Push / Blend / Evaluate
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCameraModeStack : public UObject
{
	GENERATED_BODY()

public:
	UMosesCameraModeStack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void PushCameraMode(TSubclassOf<UMosesCameraMode> CameraModeClass);
	void EvaluateStack(float DeltaTime, FMosesCameraModeView& OutCameraModeView);

private:
	UMosesCameraMode* GetCameraModeInstance(TSubclassOf<UMosesCameraMode> CameraModeClass);
	void UpdateStack(float DeltaTime);
	void BlendStack(FMosesCameraModeView& OutCameraModeView) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<UMosesCameraMode>> CameraModeInstances;

	UPROPERTY()
	TArray<TObjectPtr<UMosesCameraMode>> CameraModeStack;
};
