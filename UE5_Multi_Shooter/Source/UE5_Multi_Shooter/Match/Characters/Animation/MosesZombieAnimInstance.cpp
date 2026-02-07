#include "UE5_Multi_Shooter/Match/Characters/Animation/MosesZombieAnimInstance.h"

#include "GameFramework/Pawn.h"

UMosesZombieAnimInstance::UMosesZombieAnimInstance()
{
}

void UMosesZombieAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CacheOwner();

	// Spawn 시 기본은 살아있음
	bIsDead = false;
}

void UMosesZombieAnimInstance::SetIsDead(const bool bInDead)
{
	if (bIsDead == bInDead)
	{
		return;
	}

	bIsDead = bInDead;
}

void UMosesZombieAnimInstance::CacheOwner()
{
	if (OwnerPawn)
	{
		return;
	}

	OwnerPawn = TryGetPawnOwner();
}
