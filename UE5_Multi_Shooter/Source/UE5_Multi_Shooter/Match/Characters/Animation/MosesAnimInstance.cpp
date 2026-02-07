#include "MosesAnimInstance.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UMosesAnimInstance::UMosesAnimInstance()
{
}

void UMosesAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheOwner();
	UpdateLocomotion();
}

void UMosesAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 소유 Pawn이 교체될 수 있음(리스폰/SeamlessTravel)
	CacheOwner();
	UpdateLocomotion();
}

// ============================================================================
// [ADD] Death setter
// ============================================================================

void UMosesAnimInstance::SetIsDead(bool bInDead)
{
	if (bIsDead == bInDead)
	{
		return;
	}

	bIsDead = bInDead;
	// AnimBP는 bIsDead를 읽기만 한다.
	// 여기서 Montage를 직접 재생하거나 로직을 넣지 않는다.
}

// ============================================================================
// Internals
// ============================================================================

void UMosesAnimInstance::CacheOwner()
{
	if (OwnerPawn)
	{
		return;
	}

	OwnerPawn = TryGetPawnOwner();
}

void UMosesAnimInstance::UpdateLocomotion()
{
	if (!OwnerPawn)
	{
		Speed = 0.0f;
		bIsInAir = false;
		bIsSprinting = false;
		Direction = 0.0f;
		return;
	}

	const FVector Velocity = OwnerPawn->GetVelocity();
	Speed = Velocity.Size2D();

	const ACharacter* Char = Cast<ACharacter>(OwnerPawn);
	if (const UCharacterMovementComponent* MoveComp = Char ? Char->GetCharacterMovement() : nullptr)
	{
		bIsInAir = MoveComp->IsFalling();
	}
	else
	{
		bIsInAir = false;
	}

	// bIsSprinting은 네 프로젝트에서 Pawn의 bIsSprinting(Rep) 또는
	// Combat/Move 시스템의 값을 읽어야 함.
	// 여기서는 "소유 Pawn에 IsSprinting()이 있으면" 읽는 방식이 가장 안전함.
	// (OwnerPawn이 APlayerCharacter이면 IsSprinting() 제공)
	if (const UObject* Obj = OwnerPawn)
	{
		// 캐스팅 비용은 미미. Tick이 아니라 AnimUpdate라서 괜찮고, 필요하면 캐시해도 됨.
		const class APlayerCharacter* PC = Cast<class APlayerCharacter>(Obj);
		bIsSprinting = PC ? PC->IsSprinting() : false;
	}

	Direction = CalculateDirectionSafe(Velocity, OwnerPawn->GetActorRotation());
}

float UMosesAnimInstance::CalculateDirectionSafe(const FVector& Velocity, const FRotator& BaseRotation) const
{
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	if (Vel2D.IsNearlyZero())
	{
		return 0.0f;
	}

	const FRotator YawRot(0.f, BaseRotation.Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	const FVector NormVel = Vel2D.GetSafeNormal();

	const float ForwardDot = FVector::DotProduct(Forward, NormVel);
	const float RightDot = FVector::DotProduct(Right, NormVel);

	const float AngleRad = FMath::Atan2(RightDot, ForwardDot);
	return FMath::RadiansToDegrees(AngleRad);
}
