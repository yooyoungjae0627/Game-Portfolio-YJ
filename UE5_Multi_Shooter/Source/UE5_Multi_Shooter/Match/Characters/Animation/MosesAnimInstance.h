// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Player/Anim/MosesAnimInstance.h (FULL)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MosesAnimInstance.generated.h"

/**
 * UMosesAnimInstance
 *
 * [기능]
 * - AnimBP가 "읽기만" 할 Locomotion 변수들을 제공한다.
 *   (Speed / bIsInAir / bIsSprinting / Direction / bIsDead)
 *
 * [명세]
 * - 서버 권위 판정/게임플레이 로직 금지 (표시 전용)
 * - Tick 방식(UI 폴링) 금지 철학:
 *   - AnimInstance는 엔진이 호출하는 NativeUpdateAnimation에서
 *     소유 Pawn 상태만 읽어 변수로 제공한다.
 *
 * - KismetAnimationLibrary.h 의존 제거:
 *   - Direction 계산은 CalculateDirectionSafe()로 직접 구현한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMosesAnimInstance();

	// ---------------------------------------------------------------------
	// [ADD] Death state setter (Pawn -> AnimInstance, event-driven)
	// - CombatComponent Delegate(HandleDeadChanged)에서 1회 호출
	// - AnimBP는 이 값만 보고 AnimGraph에서 분기한다.
	// ---------------------------------------------------------------------
	void SetIsDead(bool bInDead); // [ADD]

	// (선택) BP에서 직접 세팅하고 싶으면 BlueprintCallable로 바꿔도 됨.
	// UFUNCTION(BlueprintCallable, Category="Moses|Anim")
	// void SetIsDead(bool bInDead);

protected:
	// ---------------------------
	// UAnimInstance
	// ---------------------------
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void CacheOwner();
	void UpdateLocomotion();

	/** 엔진/모듈 영향 없는 Direction 계산 */
	float CalculateDirectionSafe(const FVector& Velocity, const FRotator& BaseRotation) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> OwnerPawn = nullptr;

private:
	// ---------------------------
	// Locomotion (AnimBP reads)
	// ---------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Moses|Anim", meta = (AllowPrivateAccess = "true"))
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Moses|Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Moses|Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Moses|Anim", meta = (AllowPrivateAccess = "true"))
	float Direction = 0.0f;

	// ---------------------------------------------------------------------
	// [ADD] Death (AnimBP reads)
	// - AnimGraph 최종 Output 직전에 bIsDead로 분기해야 DeathMontage가 덮이지 않음.
	// ---------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Moses|Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false; // [ADD]
};
