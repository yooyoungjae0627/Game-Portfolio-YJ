#include "UE5_Multi_Shooter/Character/MosesCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

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

	// BeginPlay 시점에서도 한번 시도 (Seamless/PIE 케이스 보강)
	TryInitASC_FromPawn(TEXT("BeginPlay"));
}

void AMosesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// [ADDED] 서버에서 Avatar 확정 시점
	TryInitASC_FromPawn(TEXT("PossessedBy(SV)"));
}

void AMosesCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// [ADDED] 클라에서 PS 복제 도착 시점
	TryInitASC_FromPawn(TEXT("OnRep_PlayerState(CL)"));
}

void AMosesCharacter::TryInitASC_FromPawn(const TCHAR* Reason)
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	PS->TryInitASC(this);

	UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS] TryInitASC FromPawn=%s Pawn=%s PS=%s"),
		Reason ? Reason : TEXT("None"),
		*GetNameSafe(this),
		*GetNameSafe(PS));
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

	// ✅ 서버만 이동 정지 확정 (정책)
	if (!HasAuthority())
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}
}
