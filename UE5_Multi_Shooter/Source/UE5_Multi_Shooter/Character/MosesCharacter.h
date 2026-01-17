#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MosesCharacter.generated.h"

class UMosesPawnExtensionComponent;
class UMosesCameraComponent;
class UMosesHeroComponent;

class APickupBase;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMosesCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	/** [FIX] 입력 바인딩을 여기에서 확정한다. (엔진이 준비된 InputComponent를 전달함) */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override; // [FIX]

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;

	// ---------------------------
	// Input handlers (bound by HeroComponent)
	// ---------------------------
	void Input_Move(const FVector2D& MoveValue);
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_JumpPressed();
	void Input_InteractPressed();

	bool IsSprinting() const { return bIsSprinting; }

private:
	// ---------------------------
	// GAS Init
	// ---------------------------
	void TryInitASC_FromPlayerState();

	// LateJoin retry
	void StartLateJoinInitTimer();
	void StopLateJoinInitTimer();
	void LateJoinInitTick();

	// ---------------------------
	// Sprint (Server authoritative, replicated to all)
	// ---------------------------
	void ApplySprintSpeed(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	// ---------------------------
	// Pickup (Client selects target, server confirms)
	// ---------------------------
	APickupBase* FindPickupTarget_Local() const;

	UFUNCTION(Server, Reliable)
	void Server_TryPickup(APickupBase* Target);

private:
	FTimerHandle TimerHandle_LateJoinInit;
	int32 LateJoinRetryCount = 0;

	static constexpr int32 MaxLateJoinRetries = 10;
	static constexpr float LateJoinRetryIntervalSec = 0.2f;

private:
	// ---------------------------
	// Day2 tuning
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Pickup")
	float PickupTraceDistance = 500.0f;

private:
	// ---------------------------
	// Replicated State
	// ---------------------------
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesPawnExtensionComponent> PawnExtComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent;
};
