#include "UE5_Multi_Shooter/Match/Characters/Animation/AnimNotifies/AnimNotifyState_MosesZombieAttackWindow.h" // [MOD] 경로 수정

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

UAnimNotifyState_MosesZombieAttackWindow::UAnimNotifyState_MosesZombieAttackWindow()
{
}

void UAnimNotifyState_MosesZombieAttackWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* /*Animation*/,
	float /*TotalDuration*/,
	const FAnimNotifyEventReference& /*EventReference*/)
{
	if (!MeshComp)
	{
		return;
	}

	AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(MeshComp->GetOwner());
	if (!Zombie || !Zombie->HasAuthority())
	{
		return;
	}

	Zombie->ServerSetMeleeAttackWindow(AttackHand, true, bResetHitActorsOnBegin);

	UE_LOG(LogMosesZombie, Verbose, TEXT("[ZOMBIE][SV] AttackWindow Begin Hand=%d Zombie=%s"),
		static_cast<int32>(AttackHand), *Zombie->GetName());
}

void UAnimNotifyState_MosesZombieAttackWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* /*Animation*/,
	const FAnimNotifyEventReference& /*EventReference*/)
{
	if (!MeshComp)
	{
		return;
	}

	AMosesZombieCharacter* Zombie = Cast<AMosesZombieCharacter>(MeshComp->GetOwner());
	if (!Zombie || !Zombie->HasAuthority())
	{
		return;
	}

	Zombie->ServerSetMeleeAttackWindow(AttackHand, false, false);

	UE_LOG(LogMosesZombie, Verbose, TEXT("[ZOMBIE][SV] AttackWindow End Hand=%d Zombie=%s"),
		static_cast<int32>(AttackHand), *Zombie->GetName());
}
