// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraComponent.h"
#include "MosesCameraComponent.generated.h"

class UMosesCameraModeStack;
class UMosesCameraMode;

/** template forward declaration */
template <class TClass> class TSubclassOf;

/** (return type, delegate_name) */
DECLARE_DELEGATE_RetVal(TSubclassOf<UMosesCameraMode>, FMosesCameraModeDelegate);

/**
 * 
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCameraComponent : public UCameraComponent
{
	GENERATED_BODY()
	
public:
	UMosesCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	//"이 액터에 내가 원하는 타입의 컴포넌트가 붙어 있으면 찾아줘"
	// 사용 예시
	//UCameraComponent * Cam = FindComponentByClass<UCameraComponent>();
	//→ 현재 Actor에 UCameraComponent가 있으면 반환
	//→ 없으면 nullptr 반환
	static UMosesCameraComponent* FindCameraComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UMosesCameraComponent>() : nullptr); }

	/**
	* member methods
	*/
	AActor* GetTargetActor() const { return GetOwner(); }
	void UpdateCameraModes();

	/**
	 * CameraComponent interface
	 */
	virtual void OnRegister() final;
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) final;

	/** 카메라의 blending 기능을 지원하는 stack */
	UPROPERTY()
	TObjectPtr<UMosesCameraModeStack> CameraModeStack;

	/** 현재 CameraMode를 가져오는 Delegate */
	FMosesCameraModeDelegate DetermineCameraModeDelegate;
	
	
};
