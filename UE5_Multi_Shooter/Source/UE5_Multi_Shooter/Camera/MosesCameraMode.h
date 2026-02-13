// ============================================================================
// UE5_Multi_Shooter/Camera/MosesCameraMode.h  (FULL - CLEAN)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MosesCameraMode.generated.h"

class UMosesCameraComponent;
template <class TClass>
class TSubclassOf;

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
 * - Camera 결과 번들 (Location/Rotation/ControlRotation/FOV)
 */
struct FMosesCameraModeView
{
	FMosesCameraModeView();

	/** Other를 OtherWeight로 블렌드한다. */
	void Blend(const FMosesCameraModeView& Other, float OtherWeight);

	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};

/**
 * UMosesCameraMode
 *
 * 책임
 * - 특정 카메라 모드(ThirdPerson / FirstPerson / Sniper 등)의 "1프레임 View"를 계산한다.
 * - UMosesCameraComponent의 스택에 의해 생성/관리된다.
 *
 * Pitch 안전장치
 * - BP(CameraMode BP)에서 Pitch 범위를 실수로 좁혀도, 프로젝트 최소 범위(-89~+89)를 보장한다.
 */
UCLASS(Abstract, Blueprintable)
class UE5_MULTI_SHOOTER_API UMosesCameraMode : public UObject
{
	GENERATED_BODY()

public:
	UMosesCameraMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 스택 Outer에서 카메라 컴포넌트를 얻는다. */
	UMosesCameraComponent* GetMosesCameraComponent() const;

	/** 카메라 타깃 액터(보통 Pawn/Owner) */
	AActor* GetTargetActor() const;

	/** 피벗 위치(기본: PawnViewLocation / 아니면 ActorLocation) */
	FVector GetPivotLocation() const;

	/** 피벗 회전(기본: Pawn ViewRotation / 아니면 ActorRotation) */
	FRotator GetPivotRotation() const;

	/** View 계산 + 블렌딩 가중치 업데이트 */
	void UpdateCameraMode(float DeltaTime);

	/** 스택 Top으로 올라올 때 블렌딩을 새로 시작한다. */
	void ResetForNewPush(float InitialBlendWeight);

protected:
	/** 이 모드의 View를 계산한다. */
	virtual void UpdateView(float DeltaTime);

	/** 모드/프로젝트 정책을 합쳐 Pitch 유효 범위를 만든다. */
	void GetEffectivePitchLimits(float& OutMin, float& OutMax) const;

	/** PivotRot의 Pitch를 유효 범위로 Clamp한다. */
	FRotator ClampPivotRotationPitch_Safe(const FRotator& InPivotRot) const;


private:
	/** BlendTime/Function/Exponent를 이용해 BlendWeight를 갱신한다. */
	void UpdateBlending(float DeltaTime);

public:
	/** 이 모드가 계산한 최종 View */
	FMosesCameraModeView View;

public:
	// -------------------- View Tunables --------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|View",
		meta = (UIMin = "5.0", UIMax = "170.0", ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|View",
		meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMin = -89.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|View",
		meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMax = 89.0f;

public:
	// -------------------- Blending --------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Blending")
	float BlendTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Blending")
	float BlendExponent = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Blending")
	EMosesCameraModeBlendFunction BlendFunction = EMosesCameraModeBlendFunction::EaseOut;

public:
	/** 0~1: Blend 진행도 */
	float BlendAlpha = 1.0f;

	/** 0~1: 현재 모드 가중치 */
	float BlendWeight = 1.0f;

private:
	/** Pitch Clamp 로그 1회 제한 */
	mutable bool bLoggedPitchClampOnce = false;
};

/**
 * UMosesCameraModeStack
 * - CameraMode 인스턴스를 캐시하고, Push/Blend/Evaluate를 수행한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCameraModeStack : public UObject
{
	GENERATED_BODY()

public:
	UMosesCameraModeStack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 모드를 스택 Top으로 Push(필요 시 블렌딩 시작) */
	void PushCameraMode(TSubclassOf<UMosesCameraMode> CameraModeClass);

	/** 스택 업데이트 + 블렌딩을 수행하여 최종 View를 만든다. */
	void EvaluateStack(float DeltaTime, FMosesCameraModeView& OutCameraModeView);

private:
	/** 클래스에 대응하는 모드 인스턴스를 캐시에서 찾거나 생성한다. */
	UMosesCameraMode* GetCameraModeInstance(TSubclassOf<UMosesCameraMode> CameraModeClass);

	/** 각 모드 Update + 완료된 하위 모드 정리 */
	void UpdateStack(float DeltaTime);

	/** Bottom→Top으로 View 블렌딩 */
	void BlendStack(FMosesCameraModeView& OutCameraModeView) const;

private:
	/** 생성된 모드 인스턴스 캐시 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMosesCameraMode>> CameraModeInstances;

	/** 활성 스택(0=Top) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMosesCameraMode>> CameraModeStack;
};
