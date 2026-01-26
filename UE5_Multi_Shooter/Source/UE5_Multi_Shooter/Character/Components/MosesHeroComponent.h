#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MosesHeroComponent.generated.h"

class UInputComponent;
class UInputMappingContext;
class UInputAction;
class UMosesCameraMode;

/**
 * UMosesHeroComponent
 * ============================================================================
 * [로컬 전용 입력/카메라 브리지]
 *
 * - 입력 바인딩(EnhancedInput)은 "로컬 플레이어 Pawn"에서만 수행한다.
 * - Owner Pawn(PlayerCharacter)은 입력 엔드포인트(Input_*)만 제공하고,
 *   HeroComponent가 IA_*를 바인딩해 Input_*로 포워딩한다.
 *
 * [왜 이렇게?]
 * - 원격 Pawn은 입력 바인딩이 필요 없다(비용/버그 감소).
 * - Pawn은 "요청 + 코스메틱" 역할에 집중, SSOT는 PlayerState/CombatComponent가 가진다.
 *
 * [이 컴포넌트가 하는 일]
 * 1) IMC 추가(로컬 1회)
 * 2) IA_* 바인딩(로컬 1회)
 * 3) 카메라 모드 선택 Delegate 바인딩(로컬 1회)
 *
 * ============================================================================
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesHeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesHeroComponent(const FObjectInitializer& ObjectInitializer);

	/** 로컬 전용: IMC + IA 바인딩 진입점 */
	void SetupInputBindings(UInputComponent* PlayerInputComponent);

	/** 로컬 전용: MosesCameraComponent의 DetermineCameraModeDelegate 바인딩 */
	void TryBindCameraModeDelegate_LocalOnly();

protected:
	virtual void BeginPlay() override;

private:
	// =========================================================================
	// Local guard
	// =========================================================================
	bool IsLocalPlayerPawn() const;

	/** 카메라 모드 선택 Delegate 콜백 */
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

private:
	// =========================================================================
	// EnhancedInput handlers -> PlayerCharacter::Input_*
	// =========================================================================
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);

	void HandleJump(const FInputActionValue& Value);
	void HandleSprintPressed(const FInputActionValue& Value);
	void HandleSprintReleased(const FInputActionValue& Value);

	void HandleInteract(const FInputActionValue& Value);

	// Camera (optional)
	void HandleToggleView(const FInputActionValue& Value);
	void HandleAimPressed(const FInputActionValue& Value);
	void HandleAimReleased(const FInputActionValue& Value);

	// [ADD] Equip / Fire
	void HandleEquipSlot1(const FInputActionValue& Value);
	void HandleEquipSlot2(const FInputActionValue& Value);
	void HandleEquipSlot3(const FInputActionValue& Value);
	void HandleFirePressed(const FInputActionValue& Value);

private:
	// =========================================================================
	// Binding steps
	// =========================================================================
	void AddMappingContextOnce();
	void BindInputActions(UInputComponent* PlayerInputComponent);

private:
	// =========================================================================
	// Input Assets (BP에서 지정)
	// =========================================================================
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

	// Camera (optional)
	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_ToggleView = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Aim = nullptr;

	// [ADD] Equip / Fire
	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_EquipSlot1 = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_EquipSlot2 = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_EquipSlot3 = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Fire = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	int32 MappingPriority = 0;

private:
	// =========================================================================
	// Camera mode classes (BP에서 지정)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> ThirdPersonModeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> FirstPersonModeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> Sniper2xModeClass;

private:
	// =========================================================================
	// Local transient state
	// =========================================================================
	UPROPERTY(Transient)
	bool bWantsFirstPerson = false;

	UPROPERTY(Transient)
	bool bWantsSniper2x = false;

private:
	// =========================================================================
	// Runtime guards
	// =========================================================================
	bool bMappingContextAdded = false;
	bool bInputBound = false;
};
