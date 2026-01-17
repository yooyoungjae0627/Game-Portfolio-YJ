// ============================================================================
// MosesHeroComponent.h
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MosesHeroComponent.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UMosesCameraMode;

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
	bool IsLocalPlayerPawn() const;

	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

	void AddMappingContextOnce();
	void BindInputActions(UInputComponent* PlayerInputComponent);

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleSprintPressed(const FInputActionValue& Value);
	void HandleSprintReleased(const FInputActionValue& Value);
	void HandleJumpPressed(const FInputActionValue& Value);
	void HandleInteractPressed(const FInputActionValue& Value);

private:
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
	bool bMappingContextAdded = false;
	bool bInputBound = false;
};
