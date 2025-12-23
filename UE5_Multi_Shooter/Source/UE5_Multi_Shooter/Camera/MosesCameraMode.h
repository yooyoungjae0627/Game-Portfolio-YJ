// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Object.h"
#include "MosesCameraMode.generated.h"

// TPS 카메라 모드 / FPS 카메라 모드 / 차 탑승 카메라 모드...
class UMosesCameraComponent;

/**
 * [0,1]을 BlendFunction에 맞게 재매핑을 위한 타입
 */
UENUM(BlueprintType)
enum class EMosesCameraModeBlendFunction : uint8
{
	Linear,
	/**
	 * EaseIn/Out은 exponent 값에 의해 조절된다:
	 */
	EaseIn,
	EaseOut,
	EaseInOut,
	COUNT
};

struct FMosesCameraModeView
{
	FMosesCameraModeView();

	void Blend(const FMosesCameraModeView& Other, float OtherWeight);

	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};

// Camera Blending 대상 유닛
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
	void UpdateBlending(float DeltaTime);
	void UpdateCameraMode(float DeltaTime);

	virtual void UpdateView(float DeltaTime);

	/** CameraMode에 의해 생성된 CameraModeView */
	FMosesCameraModeView View;

	/** Camera Mode의 FOV */
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "5.0", UIMax = "170", ClampMin = "5.0", Clampmax = "170.0"))
	float FieldOfView;

	/** View에 대한 Pitch [Min, Max] */
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", Clampmax = "89.9"))
	float ViewPitchMin;

	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", Clampmax = "89.9"))
	float ViewPitchMax;

	/** 얼마동안 Blend를 진행할지 시간을 의미 */
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	float BlendTime;

	/** 선형적인 Blend 값 [0, 1] */
	float BlendAlpha;

	/**
	 * 해당 CameraMode의 최종 Blend 값
	 * 앞서 BlendAlpha의 선형 값을 매핑하여 최종 BlendWeight를 계산 (코드를 보며, 이해해보자)
	 */
	float BlendWeight;

	/**
	 * EaseIn/Out에 사용한 Exponent
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	float BlendExponent;

	/** Blend function */
	EMosesCameraModeBlendFunction BlendFunction;
};

// Camera Blending을 담당하는 객체
UCLASS()
class UMosesCameraModeStack : public UObject
{
	GENERATED_BODY()
public:
	UMosesCameraModeStack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UMosesCameraMode* GetCameraModeInstance(TSubclassOf<UMosesCameraMode>& CameraModeClass);
	void PushCameraMode(TSubclassOf<UMosesCameraMode>& CameraModeClass);
	void UpdateStack(float DeltaTime);
	void BlendStack(FMosesCameraModeView& OutCameraModeView) const;
	void EvaluateStack(float DeltaTime, FMosesCameraModeView& OutCameraModeView);

	// 생성된 CameraMode를 관리
	UPROPERTY()
	TArray<TObjectPtr<UMosesCameraMode>> CameraModeInstances;

	// Camera Matrix Blend 업데이트 진행 큐
	UPROPERTY()
	TArray<TObjectPtr<UMosesCameraMode>> CameraModeStack;

	
};
