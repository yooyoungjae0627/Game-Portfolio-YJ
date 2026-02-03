#include "AnimNotify_MosesSwapAttach.h"

#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UAnimNotify_MosesSwapAttach::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	const UWorld* World = MeshComp->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(MeshComp->GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	UE_LOG(LogMosesWeapon, VeryVerbose, TEXT("[SWAP][NOTIFY] Attach Animation=%s Pawn=%s"),
		*GetNameSafe(Animation),
		*GetNameSafe(PlayerChar));

	PlayerChar->HandleSwapAttachNotify();
}
