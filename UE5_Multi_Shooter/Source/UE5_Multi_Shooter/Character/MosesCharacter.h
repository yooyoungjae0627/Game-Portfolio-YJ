// ============================================================================
// MosesCharacter.h
// ============================================================================

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

public:
	/** UI/Anim에서 사용: 로컬은 예측값, 원격은 복제값 */
	bool IsSprinting() const; // [FIX] 로컬 예측 분리

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override; // [FIX] Tick 선언 누락 해결
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;

protected:
	/** [FIX] 입력 바인딩은 엔진이 준비된 InputComponent를 넘겨주는 여기에서 확정한다. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// ---------------------------
	// Input handlers (bound by HeroComponent)
	// ---------------------------
	void Input_Move(const FVector2D& MoveValue);
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_JumpPressed();
	void Input_InteractPressed();

private:
	// ---------------------------
	// GAS Init
	// ---------------------------
	void TryInitASC_FromPlayerState();

	// LateJoin retry
	void StartLateJoinInitTimer();
	void StopLateJoinInitTimer();
	void LateJoinInitTick();

private:
	// ---------------------------
	// Sprint (Server authoritative, replicated to all)
	// ---------------------------
	void ApplySprintSpeed_LocalPredict(bool bNewSprinting);          // [ADD] 로컬 예측(체감)만
	void ApplySprintSpeed_FromAuthoritativeState(const TCHAR* From); // [ADD] 서버/OnRep에서 동기화

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

private:
	// ---------------------------
	// Pickup (Client selects target, server confirms)
	// ---------------------------
	APickupBase* FindPickupTarget_Local() const;

	UFUNCTION(Server, Reliable)
	void Server_TryPickup(APickupBase* Target);

private:
	// ---------------------------
	// GAS LateJoin retry
	// ---------------------------
	FTimerHandle TimerHandle_LateJoinInit;
	int32 LateJoinRetryCount = 0;

	static constexpr int32 MaxLateJoinRetries = 10;
	static constexpr float LateJoinRetryIntervalSec = 0.2f;

private:
	// ---------------------------
	// tuning
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Pickup")
	float PickupTraceDistance = 500.0f;

private:
	// ---------------------------
	// Replicated State (SSOT: server only writes)
	// ---------------------------
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	// [ADD] 로컬 체감(예측) 전용 - 복제 금지
	UPROPERTY(Transient)
	bool bLocalPredictedSprinting = false;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesPawnExtensionComponent> PawnExtComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent;
};
