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
 * FMosesCameraModeView
 *
 * [기능]
 * - 카메라 최종 출력 값 묶음(Location/Rotation/ControlRotation/FOV)
 *
 * [명세]
 * - Blend()로 다른 View와 가중치 기반 합성 가능
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
 * [기능]
 * - TPS/FPS/Sniper 같은 "카메라 모드 1개"의 계산 단위.
 *
 * [명세]
 * - Outer는 UMosesCameraComponent여야 한다(스택이 NewObject로 생성).
 * - UpdateView()에서 View를 계산하고, UpdateBlending()에서 BlendWeight를 계산.
 * - 판정/입력 없음(표시 전용).
 */
UCLASS(Abstract, NotBlueprintable)
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

private:
	void UpdateBlending(float DeltaTime);

public:
	FMosesCameraModeView View;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|View", meta = (UIMin = "5.0", UIMax = "170.0", ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 80.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|View", meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMin = -89.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|View", meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMax = 89.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Blending")
	float BlendTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Blending")
	float BlendExponent = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Blending")
	EMosesCameraModeBlendFunction BlendFunction = EMosesCameraModeBlendFunction::EaseOut;

public:
	float BlendAlpha = 1.0f;
	float BlendWeight = 1.0f;
};

/**
 * UMosesCameraModeStack
 *
 * [기능]
 * - CameraMode들을 스택으로 쌓고, BlendWeight로 합성하여 최종 View를 만든다.
 *
 * [명세]
 * - PushCameraMode(): 모드를 Top(0)에 올림
 * - EvaluateStack(): Update + Blend로 최종 View 계산
 * - Bottom(마지막) 모드는 항상 BlendWeight=1 유지
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
