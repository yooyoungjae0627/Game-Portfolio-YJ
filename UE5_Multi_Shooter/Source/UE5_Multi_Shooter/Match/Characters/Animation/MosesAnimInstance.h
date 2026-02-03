#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MosesAnimInstance.generated.h"

/**
 * UMosesAnimInstance
 *
 * [기능]
 * - AnimBP가 "읽기만" 할 Locomotion 변수들을 제공한다.
 *   (Speed / bIsInAir / bIsSprinting / Direction)
 *
 * [명세]
 * - 서버 권위 판정/게임플레이 로직 금지 (표시 전용)
 * - Tick 방식(UI 폴링) 금지 철학: AnimInstance는 소유 Pawn 상태만 읽는다.
 * - KismetAnimationLibrary.h 의존 제거:
 *   - 일부 프로젝트/엔진 구성에서 헤더 경로 문제로 컴파일 실패할 수 있다.
 *   - 그래서 Direction 계산은 CalculateDirectionSafe()로 직접 구현한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMosesAnimInstance();

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
};
