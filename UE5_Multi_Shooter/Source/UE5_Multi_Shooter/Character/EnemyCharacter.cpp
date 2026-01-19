#include "UE5_Multi_Shooter/Character/EnemyCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

AEnemyCharacter::AEnemyCharacter()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMosesAI, Log, TEXT("[AI] Enemy BeginPlay Pawn=%s"), *GetNameSafe(this));
}
