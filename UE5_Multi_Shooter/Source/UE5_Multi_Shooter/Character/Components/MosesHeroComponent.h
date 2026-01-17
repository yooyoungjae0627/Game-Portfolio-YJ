#pragma once

#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MosesHeroComponent.generated.h"

class UInputMappingContext;
class UInputAction;
class UMosesInputComponent;
class UMosesCameraMode;

/**
 * UMosesHeroComponent
 * - 로컬 플레이어 전용: Enhanced Input MappingContext 추가 + InputAction 바인딩
 * - BeginPlay 시점에 InputComponent가 아직 준비되지 않을 수 있으므로, 타이머로 재시도한다. // [MOD]
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesHeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesHeroComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

private:
	// --- Local Pawn Helper ---
	bool IsLocalPlayerPawn() const;

	// --- Camera ---
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

	// --- Input Init ---
	void InitializePlayerInput();                 // 기존 유지
	void TryInitializePlayerInput();              // [MOD] 재시도 래퍼
	void ScheduleRetryInitializePlayerInput();    // [MOD] 타이머 재시도

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
	bool bMappingContextAdded = false; // [MOD] AddMappingContext 1회 보장
	bool bInputBound = false;          // [MOD] BindAction 1회 보장

	// ---------------- Retry (BeginPlay 타이밍 문제 방지) ----------------
	int32 InputInitAttempts = 0;             // [MOD]
	UPROPERTY()
	float InputInitRetryIntervalSec = 0.05f; // [MOD] 50ms
	UPROPERTY()
	int32 InputInitMaxAttempts = 40;         // [MOD] 40회면 2초 정도 재시도

	FTimerHandle InputInitRetryTimerHandle;  // [MOD]
};
