// ============================================================================
// UE5_Multi_Shooter/Camera/MosesCameraComponent.h  (FULL - CLEAN)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "MosesCameraComponent.generated.h"

class UMosesCameraMode;
class UMosesCameraModeStack;
struct FMosesCameraModeView;

template <class TClass>
class TSubclassOf;

/** 현재 프레임의 카메라 모드를 외부에서 결정한다. (예: HeroComponent가 바인딩) */
DECLARE_DELEGATE_RetVal(TSubclassOf<UMosesCameraMode>, FMosesCameraModeDelegate);

/**
 * UMosesCameraComponent
 *
 * 책임
 * - CameraModeStack을 통해 카메라 모드(TPS/FPS/Scope 등)를 "스택 + 블렌딩"하여 최종 View를 만든다.
 * - Tick 없이, 엔진의 GetCameraView() 호출 흐름만 사용한다.
 *
 * 처리 흐름 (GetCameraView)
 * 1) Delegate(또는 DefaultCameraModeClass)로 이번 프레임 모드를 결정하여 스택에 Push
 * 2) 스택 Evaluate(업데이트+블렌딩)로 최종 Location/Rotation/FOV 계산
 * 3) 로컬 컨트롤러만 ControlRotation 갱신 (타인/서버 영향 차단)
 * 4) DesiredView에 최종 값을 복사
 *
 * Scope(로컬 연출)
 * - 네트워크/SSOT에 관여하지 않는 "로컬 전용" 시각 효과
 * - 최종 DesiredView 단계에서만 FOV/PP(MotionBlurAmount)를 최소 삽입한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UMosesCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Actor에서 MosesCameraComponent를 찾는다. */
	static UMosesCameraComponent* FindCameraComponent(const AActor* Actor);

	/** 카메라 타깃(기본은 Owner) */
	AActor* GetTargetActor() const;

	/** 엔진 카메라 파이프라인 진입점: 최종 DesiredView를 구성한다. */
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	/** 외부에서 현재 카메라 모드를 결정하도록 바인딩하는 델리게이트 */
	FMosesCameraModeDelegate DetermineCameraModeDelegate;

public:
	/** 스나이퍼 스코프(로컬 연출) On/Off + 목표 FOV */
	void SetSniperScopeActive_Local(bool bActive, float InScopedFOV);

	/** 스코프 이동 블러 강도(0~1) */
	void SetScopeBlurStrength_Local(float InStrength01);

	/** 스코프 상태 조회(로컬) */
	bool IsSniperScopeActive_Local() const;

protected:
	/** 스택 객체를 등록 시점에 생성한다. */
	virtual void OnRegister() override final;

private:
	/** 이번 프레임에 사용할 모드를 스택에 Push한다. */
	void UpdateCameraModes();

	/** Delegate 우선, 없으면 DefaultCameraModeClass로 폴백한다. */
	TSubclassOf<UMosesCameraMode> ResolveCameraModeClass() const;

	/** 로컬 컨트롤러만 ControlRotation을 갱신한다. */
	void UpdateLocalControlRotationIfNeeded(const FMosesCameraModeView& CameraModeView);

	/** 계산된 View를 DesiredView로 복사한다. (스코프 오버라이드 포함) */
	void ApplyCameraViewToDesiredView(const FMosesCameraModeView& CameraModeView, FMinimalViewInfo& DesiredView);

private:
	/** 카메라 모드 스택(스택/블렌딩 수행) */
	UPROPERTY(Transient)
	TObjectPtr<UMosesCameraModeStack> CameraModeStack = nullptr;

	/** Delegate 미바인딩 시 사용할 기본 모드 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> DefaultCameraModeClass;

private:
	/** 델리게이트 미바인딩 로그 1회 제한 */
	mutable bool bLoggedNoDelegateOnce = false;

	/** ModeClass 미결정 로그 1회 제한 */
	mutable bool bLoggedNoModeClassOnce = false;

private:
	/** 스코프 활성 여부(로컬) */
	UPROPERTY(Transient)
	bool bScopeActive_Local = false;

	/** 스코프 FOV(로컬) */
	UPROPERTY(Transient)
	float ScopedFOV_Local = 45.0f;

	/** 스코프 이동 블러 강도(로컬 0~1) */
	UPROPERTY(Transient)
	float ScopeBlurStrength_Local = 0.0f;
};
