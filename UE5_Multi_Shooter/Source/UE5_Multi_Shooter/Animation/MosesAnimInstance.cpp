#include "UE5_Multi_Shooter/Animation/MosesAnimInstance.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

UMosesAnimInstance::UMosesAnimInstance()
{
	// AnimInstance는 기본적으로 Tick과는 별개로 NativeUpdateAnimation이 호출된다.
}

void UMosesAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 초기 소유 Pawn 캐시
	CacheOwner();
}

void UMosesAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 레벨 이동/리스폰 등으로 Pawn이 바뀔 수 있으니 null이면 재캐시
	if (!OwnerPawn)
	{
		CacheOwner();
	}

	// 실제 Locomotion 값 업데이트
	UpdateLocomotion();
}

void UMosesAnimInstance::CacheOwner()
{
	OwnerPawn = TryGetPawnOwner();
}

void UMosesAnimInstance::UpdateLocomotion()
{
	if (!OwnerPawn)
	{
		// 안전 초기화
		Speed = 0.0f;
		bIsInAir = false;
		bIsSprinting = false;
		Direction = 0.0f;
		return;
	}

	// 1) Speed: 지면(XY) 속도만 계산
	const FVector Velocity = OwnerPawn->GetVelocity();
	Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

	// 2) InAir: CharacterMovement의 Falling 여부
	const ACharacter* Char = Cast<ACharacter>(OwnerPawn);
	const UCharacterMovementComponent* MoveComp = Char ? Char->GetCharacterMovement() : nullptr;
	bIsInAir = MoveComp ? MoveComp->IsFalling() : false;

	// 3) Sprint: PlayerCharacter만 true 가능(Enemy는 false)
	const APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(OwnerPawn);
	bIsSprinting = PlayerChar ? PlayerChar->IsSprinting() : false;

	// 4) Direction: KismetAnimationLibrary 없이 안전 계산
	Direction = CalculateDirectionSafe(Velocity, OwnerPawn->GetActorRotation());
}

float UMosesAnimInstance::CalculateDirectionSafe(const FVector& Velocity, const FRotator& BaseRotation) const
{
	const FVector HorizontalVel(Velocity.X, Velocity.Y, 0.0f);
	if (HorizontalVel.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// 기준 회전에서 Forward/Right 축을 얻고, 속도 방향과 내적해서 각도를 만든다.
	const FVector Forward = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::Y);
	const FVector Dir = HorizontalVel.GetSafeNormal();

	const float ForwardCos = FVector::DotProduct(Forward, Dir);
	const float RightCos = FVector::DotProduct(Right, Dir);

	// atan2(right, forward) → -180 ~ 180
	return FMath::RadiansToDegrees(FMath::Atan2(RightCos, ForwardCos));
}
