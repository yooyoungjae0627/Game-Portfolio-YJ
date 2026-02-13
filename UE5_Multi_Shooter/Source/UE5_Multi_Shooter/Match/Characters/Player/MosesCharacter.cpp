// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Player/MosesCharacter.cpp
// ============================================================================

#include "UE5_Multi_Shooter/Match/Characters/Player/MosesCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesPawnSSOTGuardComponent.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

// =========================================================
// Constructor
// =========================================================

AMosesCharacter::AMosesCharacter()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	// Pawn SSOT 위반 감지기
	PawnSSOTGuard =
		CreateDefaultSubobject<UMosesPawnSSOTGuardComponent>(TEXT("PawnSSOTGuard"));
}

// =========================================================
// Engine Lifecycle
// =========================================================

void AMosesCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = DefaultWalkSpeed;
	}

	UE_LOG(LogMosesMove, Log,
		TEXT("[MOVE] MosesCharacter BeginPlay Pawn=%s Walk=%.0f"),
		*GetNameSafe(this),
		DefaultWalkSpeed);

	// Seamless / PIE 케이스 보강
	TryInitASC_FromPawn(TEXT("BeginPlay"));
}

void AMosesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 Avatar 확정 시점
	TryInitASC_FromPawn(TEXT("PossessedBy(SV)"));
}

void AMosesCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트에서 PlayerState 복제 도착 시점
	TryInitASC_FromPawn(TEXT("OnRep_PlayerState(CL)"));
}

// =========================================================
// GAS Init Helper
// =========================================================

void AMosesCharacter::TryInitASC_FromPawn(const TCHAR* Reason)
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	PS->TryInitASC(this);

	UE_LOG(LogMosesGAS, Verbose,
		TEXT("[GAS] TryInitASC FromPawn=%s Pawn=%s PS=%s"),
		Reason ? Reason : TEXT("None"),
		*GetNameSafe(this),
		*GetNameSafe(PS));
}

// =========================================================
// Server Authority Hooks
// =========================================================

void AMosesCharacter::HandleDamaged(float DamageAmount, AActor* DamageCauser)
{
	UE_LOG(LogMosesCombat, Log,
		TEXT("[Damage] Pawn=%s Amount=%.2f Causer=%s"),
		*GetNameSafe(this),
		DamageAmount,
		*GetNameSafe(DamageCauser));
}

void AMosesCharacter::HandleDeath(AActor* DeathCauser)
{
	UE_LOG(LogMosesCombat, Warning,
		TEXT("[Death] Pawn=%s Causer=%s"),
		*GetNameSafe(this),
		*GetNameSafe(DeathCauser));

	if (!HasAuthority())
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}
}

// =========================================================
// Server Pipeline Wrappers
// =========================================================

void AMosesCharacter::ServerNotifyDamaged(float DamageAmount, AActor* DamageCauser)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted ServerNotifyDamaged"));
	check(HasAuthority());

	HandleDamaged(DamageAmount, DamageCauser);
}

void AMosesCharacter::ServerNotifyDeath(AActor* DeathCauser)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted ServerNotifyDeath"));
	check(HasAuthority());

	HandleDeath(DeathCauser);
}

// =========================================================
// Cosmetic Triggers (All Clients)
// =========================================================

void AMosesCharacter::Multicast_PlayHitReactCosmetic_Implementation()
{
	UE_LOG(LogMosesCombat, Verbose,
		TEXT("[HIT][COS] PlayHitReact Pawn=%s"),
		*GetNameSafe(this));
}

void AMosesCharacter::Multicast_PlayDeathCosmetic_Implementation()
{
	UE_LOG(LogMosesCombat, Warning,
		TEXT("[DEATH][COS] PlayDeath Pawn=%s"),
		*GetNameSafe(this));
}
