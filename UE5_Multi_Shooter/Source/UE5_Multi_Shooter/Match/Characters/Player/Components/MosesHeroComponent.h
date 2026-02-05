#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MosesHeroComponent.generated.h"

class UInputComponent;
class UInputMappingContext;
class UInputAction;
class UMosesCameraMode;

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
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

private:
	void AddMappingContextOnce();
	void BindInputActions(UInputComponent* PlayerInputComponent);

private:
	// ---- Input Handlers ----
	void HandleMove(const FInputActionValue& Value);
	void HandleLookYaw(const FInputActionValue& Value);
	void HandleLookPitch(const FInputActionValue& Value);

	void HandleJump(const FInputActionValue& Value);
	void HandleSprintPressed(const FInputActionValue& Value);
	void HandleSprintReleased(const FInputActionValue& Value);

	void HandleInteract(const FInputActionValue& Value);

	void HandleToggleView(const FInputActionValue& Value);
	void HandleAimPressed(const FInputActionValue& Value);
	void HandleAimReleased(const FInputActionValue& Value);

	void HandleEquipSlot1(const FInputActionValue& Value);
	void HandleEquipSlot2(const FInputActionValue& Value);
	void HandleEquipSlot3(const FInputActionValue& Value);
	void HandleEquipSlot4(const FInputActionValue& Value); // [MOD][SLOT4]

	void HandleReload(const FInputActionValue& Value);      // [MOD][RELOAD]

	void HandleFirePressed(const FInputActionValue& Value);
	void HandleFireReleased(const FInputActionValue& Value);

private:
	// ---- Input Assets ----
	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_LookYaw = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_LookPitch = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Jump = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Sprint = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Interact = nullptr;

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
	TObjectPtr<UInputAction> IA_EquipSlot4 = nullptr; // [MOD][SLOT4]

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Reload = nullptr;     // [MOD][RELOAD]

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	TObjectPtr<UInputAction> IA_Fire = nullptr;

	UPROPERTY(EditAnywhere, Category = "Moses|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditAnywhere, Category = "Moses|Input|Look", meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float LookSensitivityYaw = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Moses|Input|Look", meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float LookSensitivityPitch = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Moses|Input|Look")
	bool bInvertPitch = false;

private:
	// ---- Camera Modes ----
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> ThirdPersonModeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> FirstPersonModeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> Sniper2xModeClass;

private:
	UPROPERTY(Transient)
	bool bWantsFirstPerson = false;

	UPROPERTY(Transient)
	bool bWantsSniper2x = false;

private:
	bool bMappingContextAdded = false;
	bool bInputBound = false;
};
