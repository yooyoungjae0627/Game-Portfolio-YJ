#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MosesHeroComponent.generated.h"

class UInputMappingContext;
class UInputAction;
class UMosesCameraMode;

/**
 * UMosesHeroComponent
 *
 * [기능]
 * - 로컬 플레이어 전용:
 *   1) Enhanced Input 바인딩(IMC + IA들)
 *   2) 카메라 컴포넌트 Delegate 바인딩(현재 CameraMode 선택)
 *   3) 입력을 PlayerCharacter의 Input_* 엔드포인트로 포워딩
 *
 * [명세]
 * - 로컬 컨트롤러(내 화면)에서만 동작한다.
 * - Enemy에는 붙어도 되지만(권장X), 로컬이 아니면 아무 일도 하지 않는다.
 *
 * [카메라 모드 선택 규칙(간단 버전)]
 * - V키(토글)로 TPS/FPS를 바꾸고
 * - 우클릭(누르는 동안)으로 Sniper2x를 켠다(예시)
 *
 * ※ 입력 액션(IA_ToggleView, IA_Aim)은 BP에서 세팅하면 된다.
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesHeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesHeroComponent(const FObjectInitializer& ObjectInitializer);

	void SetupInputBindings(UInputComponent* PlayerInputComponent);
	
	void TryBindCameraModeDelegate_LocalOnly();

protected:
	virtual void BeginPlay() override;

private:
	bool IsLocalPlayerPawn() const;

	/** 카메라 컴포넌트가 호출하는 Delegate: 현재 카메라 모드 클래스를 반환 */
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

private:
	// 입력 핸들러
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJump(const FInputActionValue& Value);
	void HandleSprintPressed(const FInputActionValue& Value);
	void HandleSprintReleased(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);

	void HandleToggleView(const FInputActionValue& Value); // TPS/FPS 토글
	void HandleAimPressed(const FInputActionValue& Value); // Sniper2x On
	void HandleAimReleased(const FInputActionValue& Value); // Sniper2x Off

private:
	void AddMappingContextOnce();
	void BindInputActions(UInputComponent* PlayerInputComponent);

private:
	// ---------------------------
	// Input Assets (BP에서 지정)
	// ---------------------------
	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Look = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Jump = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Sprint = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Interact = nullptr;

	// 카메라 토글/조준(선택)
	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_ToggleView = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Aim = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_EquipSlot1 = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_EquipSlot2 = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_EquipSlot3 = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	int32 MappingPriority = 0;

private:
	// ---------------------------
	// Camera Mode Classes (BP에서 지정 가능)
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> ThirdPersonModeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> FirstPersonModeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> Sniper2xModeClass;

private:
	// 로컬 상태(카메라 선택)
	UPROPERTY(Transient)
	bool bWantsFirstPerson = false;

	UPROPERTY(Transient)
	bool bWantsSniper2x = false;

private:
	bool bMappingContextAdded = false;
	bool bInputBound = false;
};
