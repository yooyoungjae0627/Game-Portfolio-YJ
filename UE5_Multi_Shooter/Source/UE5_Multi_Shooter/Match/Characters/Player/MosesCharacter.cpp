#include "UE5_Multi_Shooter/Match/Characters/Player/MosesCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesPawnSSOTGuardComponent.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

AMosesCharacter::AMosesCharacter()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	// Pawn에 전투 SSOT를 넣지 말라는 정책을 자동 감지
	PawnSSOTGuard = CreateDefaultSubobject<UMosesPawnSSOTGuardComponent>(TEXT("PawnSSOTGuard"));
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

	// BeginPlay에서도 ASC 초기화 보강 (Seamless/PIE 케이스)
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

	// 클라에서 PS 복제 도착 시점
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

	if (!HasAuthority())
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}
}

// ============================================================================
// Server pipeline wrappers
// ============================================================================

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

// ============================================================================
// Cosmetic triggers (All clients)
// ============================================================================

void AMosesCharacter::Multicast_PlayHitReactCosmetic_Implementation()
{
	UE_LOG(LogMosesCombat, Verbose, TEXT("[HIT][COS] PlayHitReact Pawn=%s"), *GetNameSafe(this));
}

void AMosesCharacter::Multicast_PlayDeathCosmetic_Implementation()
{
	UE_LOG(LogMosesCombat, Warning, TEXT("[DEATH][COS] PlayDeath Pawn=%s"), *GetNameSafe(this));
}
