#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "MosesCameraComponent.generated.h"

class UMosesCameraModeStack;
class UMosesCameraMode;
template <class TClass> class TSubclassOf;

/** 현재 프레임의 카메라 모드를 외부에서 결정 */
DECLARE_DELEGATE_RetVal(TSubclassOf<UMosesCameraMode>, FMosesCameraModeDelegate);

/**
 * UMosesCameraComponent
 *
 * [기능]
 * - CameraModeStack을 통해 TPS/FPS/Sniper2x 같은 모드를 "블렌딩"하여 최종 카메라 View를 만든다.
 * - 엔진이 GetCameraView()를 호출할 때마다:
 *   1) Delegate(또는 Default)로 모드를 결정하고 스택에 Push
 *   2) 스택 Evaluate → 최종 Location/Rotation/FOV 산출
 *   3) 로컬 컨트롤러만 ControlRotation 갱신
 *   4) DesiredView에 값 세팅
 *
 * [명세]
 * - Tick 사용 안 함 (GetCameraView 호출 흐름 사용)
 * - Delegate 미바인딩 시 DefaultCameraModeClass로 폴백(화면 안정)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UMosesCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UMosesCameraComponent* FindCameraComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UMosesCameraComponent>() : nullptr);
	}

	AActor* GetTargetActor() const { return GetOwner(); }

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

protected:
	virtual void OnRegister() override final;

private:
	void UpdateCameraModes();
	TSubclassOf<UMosesCameraMode> ResolveCameraModeClass() const;

	void UpdateLocalControlRotationIfNeeded(const struct FMosesCameraModeView& CameraModeView);
	void ApplyCameraViewToDesiredView(const struct FMosesCameraModeView& CameraModeView, FMinimalViewInfo& DesiredView);

public:
	/** 외부(HeroComponent)가 바인딩해서 현재 카메라 모드 클래스를 반환 */
	FMosesCameraModeDelegate DetermineCameraModeDelegate;

private:
	UPROPERTY()
	TObjectPtr<UMosesCameraModeStack> CameraModeStack = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> DefaultCameraModeClass;

private:
	mutable bool bLoggedNoDelegateOnce = false;
	mutable bool bLoggedNoModeClassOnce = false;
};
