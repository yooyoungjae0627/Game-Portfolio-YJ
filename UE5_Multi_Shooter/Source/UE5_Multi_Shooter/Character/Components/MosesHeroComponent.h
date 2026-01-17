#pragma once

#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MosesHeroComponent.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent; // [FIX] 표준 EnhancedInputComponent 사용
class UMosesCameraMode;

/**
 * UMosesHeroComponent
 * - 로컬 플레이어 전용:
 *   1) CameraModeDelegate 바인딩
 *   2) Enhanced Input MappingContext 추가(1회)
 *   3) InputAction 바인딩 (Pawn::SetupPlayerInputComponent에서 호출)
 *
 * [FIX]
 * - BindAction 대상은 UMosesInputComponent(커스텀) 강제가 아니라
 *   UEnhancedInputComponent(엔진 표준)를 사용한다.
 *   -> "PawnInputComponent0"로 들어오는 케이스에서도 입력 바인딩이 안정적으로 동작.
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesHeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesHeroComponent(const FObjectInitializer& ObjectInitializer);

public:
	/** Pawn::SetupPlayerInputComponent에서 호출 */
	void SetupInputBindings(UInputComponent* PlayerInputComponent);

protected:
	virtual void BeginPlay() override;

private:
	// --- Local Pawn Helper ---
	bool IsLocalPlayerPawn() const;

	// --- Camera ---
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

	// --- Input ---
	void AddMappingContextOnce();
	void BindInputActions(UInputComponent* PlayerInputComponent);

	// --- Input Handlers ---
	void HandleMove(const FInputActionValue& Value);
	void HandleSprintPressed(const FInputActionValue& Value);
	void HandleSprintReleased(const FInputActionValue& Value);
	void HandleJumpPressed(const FInputActionValue& Value);
	void HandleInteractPressed(const FInputActionValue& Value);

private:
	// ---------------- Input Assets (BP에서 지정) ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Sprint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Jump = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Interact = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moses|Input", meta = (AllowPrivateAccess = "true"))
	int32 MappingPriority = 0;

private:
	// ---------------- Runtime Guards ----------------
	bool bMappingContextAdded = false;
	bool bInputBound = false;
};
