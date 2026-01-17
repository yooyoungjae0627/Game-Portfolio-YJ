#include "UE5_Multi_Shooter/Animation/MosesAnimInstance.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "UE5_Multi_Shooter/Character/MosesCharacter.h"

namespace MosesAnim
{
	/**
	 * UKismetAnimationLibrary::CalculateDirection 대체 구현.
	 * - Velocity가 0이면 0 반환
	 * - Forward 기준 각도(-180~180)를 반환
	 */
	static float CalculateDirectionDegrees(const FVector& Velocity, const FRotator& BaseRotation)
	{
		const FVector LateralVelocity(Velocity.X, Velocity.Y, 0.f);
		if (LateralVelocity.IsNearlyZero())
		{
			return 0.f;
		}

		const FVector Forward = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::Y);

		const FVector MoveDir = LateralVelocity.GetSafeNormal();

		const float ForwardCos = FVector::DotProduct(Forward, MoveDir);
		const float RightCos = FVector::DotProduct(Right, MoveDir);

		// atan2(Right, Forward) -> degrees
		return FMath::RadiansToDegrees(FMath::Atan2(RightCos, ForwardCos));
	}
}

UMosesAnimInstance::UMosesAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedPawn = TryGetPawnOwner();
}

void UMosesAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = CachedPawn.IsValid() ? CachedPawn.Get() : TryGetPawnOwner();
	if (!Pawn)
	{
		return;
	}

	const FVector Velocity = Pawn->GetVelocity();
	const FVector LateralVelocity(Velocity.X, Velocity.Y, 0.f);
	Speed = LateralVelocity.Size();

	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
	bIsInAir = MoveComp ? MoveComp->IsFalling() : false;

	Direction = MosesAnim::CalculateDirectionDegrees(Velocity, Pawn->GetActorRotation());

	const AMosesCharacter* MosesChar = Cast<AMosesCharacter>(Pawn);
	bIsSprinting = MosesChar ? MosesChar->IsSprinting() : false;
}
