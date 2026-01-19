#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "PlayerCharacter.generated.h"

class UMosesHeroComponent;
class UMosesCameraComponent;
class APickupBase;

/**
 * APlayerCharacter
 *
 * [기능]
 * - 로컬 입력 엔드포인트(이동/시점/점프/스프린트/상호작용).
 * - 스프린트는 서버 권위로 확정(RepNotify)하고, 로컬은 예측값으로 즉시 반응.
 * - 픽업은 로컬이 타겟을 찾고 → 서버 RPC → PickupBase가 원자성/거리 체크 후 확정.
 *
 * [명세]
 * - HeroComponent가 Input_* 함수를 호출한다(그래서 public).
 * - 카메라 시작점은 FollowCamera가 아니라 UMosesCameraComponent에서 가져온다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API APlayerCharacter : public AMosesCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	bool IsSprinting() const;

	// 입력 엔드포인트(HeroComponent 호출)
	void Input_Move(const FVector2D& MoveValue);
	void Input_Look(const FVector2D& LookValue);
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_JumpPressed();
	void Input_InteractPressed();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// Sprint
	void ApplySprintSpeed_LocalPredict(bool bNewSprinting);
	void ApplySprintSpeed_FromAuth(const TCHAR* From);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	// Pickup
	APickupBase* FindPickupTarget_Local() const;

	UFUNCTION(Server, Reliable)
	void Server_TryPickup(APickupBase* Target);

private:
	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> MosesCameraComponent = nullptr;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Pickup")
	float PickupTraceDistance = 500.0f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(Transient)
	bool bLocalPredictedSprinting = false;
};
