#include "UE5_Multi_Shooter/Character/MosesCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

AMosesCharacter::AMosesCharacter()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AMosesCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = DefaultWalkSpeed;
	}

	UE_LOG(LogMosesMove, Log, TEXT("[MOVE] MosesCharacter BeginPlay Pawn=%s Walk=%.0f"),
		*GetNameSafe(this), DefaultWalkSpeed);
}

void AMosesCharacter::HandleDamaged(float DamageAmount, AActor* DamageCauser)
{
	UE_LOG(LogMosesCombat, Log, TEXT("[Damage] Pawn=%s Amount=%.2f Causer=%s"),
		*GetNameSafe(this), DamageAmount, *GetNameSafe(DamageCauser));
}

void AMosesCharacter::HandleDeath(AActor* DeathCauser)
{
	UE_LOG(LogMosesCombat, Warning, TEXT("[Death] Pawn=%s Causer=%s"),
		*GetNameSafe(this), *GetNameSafe(DeathCauser));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}
}
