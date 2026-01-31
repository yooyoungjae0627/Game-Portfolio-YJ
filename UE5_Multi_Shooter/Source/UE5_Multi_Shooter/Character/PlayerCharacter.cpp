#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Player/MosesInteractionComponent.h" 

#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "TimerManager.h"

// ============================================================================
// Local helpers
// ============================================================================

static void Moses_ApplyRotationPolicy(ACharacter* Char, const TCHAR* FromTag)
{
	if (!Char)
	{
		return;
	}

	Char->bUseControllerRotationYaw = true;
	Char->bUseControllerRotationPitch = false;
	Char->bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->RotationRate = FRotator(0.f, 720.f, 0.f);

		UE_LOG(LogMosesMove, Log,
			TEXT("[ROT][%s] ApplyPolicy OK OrientToMove=%d UseCtrlDesired=%d RotRateYaw=%.0f Pawn=%s"),
			FromTag ? FromTag : TEXT("UNK"),
			MoveComp->bOrientRotationToMovement ? 1 : 0,
			MoveComp->bUseControllerDesiredRotation ? 1 : 0,
			MoveComp->RotationRate.Yaw,
			*GetNameSafe(Char));
	}
}

// ============================================================================
// Ctor / Lifecycle
// ============================================================================

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));

	MosesCameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("MosesCameraComponent"));
	MosesCameraComponent->SetupAttachment(GetRootComponent());

	// [MOD] InteractionComponent 생성 (E Hold 라우팅)
	InteractionComponent = CreateDefaultSubobject<UMosesInteractionComponent>(TEXT("InteractionComponent"));

	WeaponMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh());
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComp->SetGenerateOverlapEvents(false);
	WeaponMeshComp->SetIsReplicated(false);

	Moses_ApplyRotationPolicy(this, TEXT("Ctor"));
}

void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (WeaponMeshComp && GetMesh())
	{
		WeaponMeshComp->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CharacterWeaponSocketName);

		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][COS] PostInit Attach Socket=%s Pawn=%s"),
			*CharacterWeaponSocketName.ToString(),
			*GetNameSafe(this));
	}

	Moses_ApplyRotationPolicy(this, TEXT("PostInit"));
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	Moses_ApplyRotationPolicy(this, TEXT("BeginPlay"));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}

	BindCombatComponent();
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAutoFire_Local();
	UnbindCombatComponent();

	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!ensure(PlayerInputComponent))
	{
		return;
	}

	if (!HeroComponent)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero][Input] No HeroComponent. Pawn=%s"), *GetNameSafe(this));
		return;
	}

	HeroComponent->SetupInputBindings(PlayerInputComponent);
	HeroComponent->TryBindCameraModeDelegate_LocalOnly();

	// 개발자 주석:
	// - Interact Press/Release 바인딩은 HeroComponent 내부(EnhancedInput)에서
	//   APlayerCharacter::Input_InteractPressed/Released를 호출하는 형태를 권장한다.
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
}

// ============================================================================
// Possession / Restart hooks
// ============================================================================

void APlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	Moses_ApplyRotationPolicy(this, TEXT("OnRep_Controller"));
	BindCombatComponent();
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	Moses_ApplyRotationPolicy(this, TEXT("PawnClientRestart"));
	BindCombatComponent();
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	Moses_ApplyRotationPolicy(this, TEXT("PossessedBy"));
	BindCombatComponent();
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindCombatComponent();
}

// ============================================================================
// Query
// ============================================================================

bool APlayerCharacter::IsSprinting() const
{
	return bIsSprinting;
}

// ============================================================================
// Input Endpoints
// ============================================================================

void APlayerCharacter::Input_Move(const FVector2D& MoveValue)
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (Controller == nullptr)
	{
		return;
	}

	LastMoveInputLocal = MoveValue;

	const FRotator YawRot(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MoveValue.Y);
	AddMovementInput(Right, MoveValue.X);

	UpdateSprintRequest_Local(TEXT("Move"));
}

void APlayerCharacter::Input_Look(const FVector2D& LookValue)
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void APlayerCharacter::Input_SprintPressed()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	bSprintKeyDownLocal = true;
	UpdateSprintRequest_Local(TEXT("SprintPressed"));
}

void APlayerCharacter::Input_SprintReleased()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	bSprintKeyDownLocal = false;
	UpdateSprintRequest_Local(TEXT("SprintReleased"));
}

void APlayerCharacter::Input_JumpPressed()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	Jump();
}

// [MOD] Interact Press/Release -> InteractionComponent 라우팅
void APlayerCharacter::Input_InteractPressed()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (InteractionComponent)
	{
		InteractionComponent->RequestInteractPressed();
	}
}

void APlayerCharacter::Input_InteractReleased()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (InteractionComponent)
	{
		InteractionComponent->RequestInteractReleased();
	}
}

void APlayerCharacter::Input_EquipSlot1()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(1);
	}
}

void APlayerCharacter::Input_EquipSlot2()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(2);
	}
}

void APlayerCharacter::Input_EquipSlot3()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(3);
	}
}

// ============================================================================
// Hold-to-fire (Local only)
// ============================================================================

void APlayerCharacter::Input_FirePressed()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (bFireHeldLocal)
	{
		return;
	}

	bFireHeldLocal = true;

	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] FirePressed -> Held=1 Pawn=%s"), *GetNameSafe(this));

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestFire();
	}

	StartAutoFire_Local();
}

void APlayerCharacter::Input_FireReleased()
{
	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] FireReleased Pawn=%s"), *GetNameSafe(this));
	StopAutoFire_Local();
}

void APlayerCharacter::StartAutoFire_Local()
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire START blocked (NotLocal) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(AutoFireTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		AutoFireTimerHandle,
		this,
		&ThisClass::HandleAutoFireTick_Local,
		AutoFireTickRate,
		true,
		AutoFireTickRate);

	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire START OK Rate=%.3f Pawn=%s"),
		AutoFireTickRate,
		*GetNameSafe(this));
}

void APlayerCharacter::StopAutoFire_Local()
{
	if (!bFireHeldLocal)
	{
		return;
	}

	bFireHeldLocal = false;
	GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);

	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire STOP OK Pawn=%s"), *GetNameSafe(this));
}

void APlayerCharacter::HandleAutoFireTick_Local()
{
	if (!bFireHeldLocal)
	{
		return;
	}

	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		StopAutoFire_Local();
		return;
	}

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestFire();
	}
}

// ============================================================================
// Sprint (Server authoritative + Shift+WASD gating)
// ============================================================================

void APlayerCharacter::UpdateSprintRequest_Local(const TCHAR* FromTag)
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const bool bHasMoveInput = (LastMoveInputLocal.SizeSquared() > 0.0001f);
	const bool bWantsSprint = (bSprintKeyDownLocal && bHasMoveInput);

	ApplySprintSpeed_LocalPredict(bWantsSprint);

	if (bIsSprinting != bWantsSprint)
	{
		Server_SetSprinting(bWantsSprint);

		UE_LOG(LogMosesMove, Verbose, TEXT("[MOVE][CL] SprintRequest=%d From=%s Pawn=%s"),
			bWantsSprint ? 1 : 0,
			FromTag ? FromTag : TEXT("UNK"),
			*GetNameSafe(this));
	}
}

void APlayerCharacter::ApplySprintSpeed_LocalPredict(bool bNewSprinting)
{
	bLocalPredictedSprinting = bNewSprinting;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bNewSprinting ? SprintSpeed : WalkSpeed;
	}
}

void APlayerCharacter::ApplySprintSpeed_FromAuth(const TCHAR* From)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	UE_LOG(LogMosesMove, Log, TEXT("[MOVE][%s] Sprint Auth=%d Speed=%.0f Pawn=%s"),
		From ? From : TEXT("UNK"),
		bIsSprinting ? 1 : 0,
		GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : -1.f,
		*GetNameSafe(this));
}

void APlayerCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Move", TEXT("Client attempted Server_SetSprinting"));

	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;

	ApplySprintSpeed_FromAuth(TEXT("SV"));
	Multicast_PlaySprintMontage(bIsSprinting);

	OnRep_IsSprinting();
}

void APlayerCharacter::OnRep_IsSprinting()
{
	ApplySprintSpeed_FromAuth(TEXT("CL"));
}

void APlayerCharacter::Multicast_PlaySprintMontage_Implementation(bool bStart)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!SprintMontage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		return;
	}

	if (bStart)
	{
		if (AnimInst->Montage_IsPlaying(SprintMontage))
		{
			AnimInst->Montage_Stop(0.0f, SprintMontage);
		}

		AnimInst->Montage_Play(SprintMontage);
		UE_LOG(LogMosesMove, Verbose, TEXT("[MOVE][COS] SprintMontage PLAY Pawn=%s"), *GetNameSafe(this));
	}
	else
	{
		AnimInst->Montage_Stop(0.1f, SprintMontage);
		UE_LOG(LogMosesMove, Verbose, TEXT("[MOVE][COS] SprintMontage STOP Pawn=%s"), *GetNameSafe(this));
	}
}

// ============================================================================
// CombatComponent Bind (RepNotify -> Delegate -> Cosmetic)
// ============================================================================

void APlayerCharacter::BindCombatComponent()
{
	UMosesCombatComponent* NewComp = GetCombatComponent_Checked();
	if (!NewComp)
	{
		return;
	}

	if (CachedCombatComponent == NewComp)
	{
		return;
	}

	UnbindCombatComponent();

	CachedCombatComponent = NewComp;

	CachedCombatComponent->OnEquippedChanged.AddUObject(this, &APlayerCharacter::HandleEquippedChanged);
	CachedCombatComponent->OnDeadChanged.AddUObject(this, &APlayerCharacter::HandleDeadChanged);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][Bind] CombatComponent Bound Pawn=%s PS=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPlayerState()));

	HandleEquippedChanged(CachedCombatComponent->GetCurrentSlot(), CachedCombatComponent->GetEquippedWeaponId());
	HandleDeadChanged(CachedCombatComponent->IsDead());
}

void APlayerCharacter::UnbindCombatComponent()
{
	if (!CachedCombatComponent)
	{
		return;
	}

	CachedCombatComponent->OnEquippedChanged.RemoveAll(this);
	CachedCombatComponent->OnDeadChanged.RemoveAll(this);

	CachedCombatComponent = nullptr;
}

UMosesCombatComponent* APlayerCharacter::GetCombatComponent_Checked() const
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return nullptr;
	}

	return PS->FindComponentByClass<UMosesCombatComponent>();
}

void APlayerCharacter::HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId)
{
	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS] EquippedChanged Slot=%d Weapon=%s Pawn=%s"),
		SlotIndex,
		*WeaponId.ToString(),
		*GetNameSafe(this));

	RefreshWeaponCosmetic(WeaponId);
}

void APlayerCharacter::HandleDeadChanged(bool bNewDead)
{
	UE_LOG(LogMosesCombat, Log, TEXT("[DEAD][COS] DeadChanged bDead=%d Pawn=%s"),
		bNewDead ? 1 : 0,
		*GetNameSafe(this));

	if (!bNewDead)
	{
		return;
	}

	if (HasAuthority())
	{
		Multicast_PlayDeathMontage();
	}

	ApplyDeadCosmetics_Local();
}

void APlayerCharacter::RefreshWeaponCosmetic(FGameplayTag WeaponId)
{
	if (!WeaponId.IsValid() || !WeaponMeshComp)
	{
		return;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}

	const UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data || Data->WeaponSkeletalMesh.IsNull())
	{
		return;
	}

	USkeletalMesh* LoadedMesh = Data->WeaponSkeletalMesh.Get();
	if (!LoadedMesh)
	{
		LoadedMesh = Data->WeaponSkeletalMesh.LoadSynchronous();
	}
	if (!LoadedMesh)
	{
		return;
	}

	WeaponMeshComp->SetSkeletalMesh(LoadedMesh);

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		WeaponMeshComp->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CharacterWeaponSocketName);
	}
}

// ============================================================================
// Montage Cosmetics
// ============================================================================

void APlayerCharacter::TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const
{
	if (!Montage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		return;
	}

	if (AnimInst->Montage_IsPlaying(Montage))
	{
		AnimInst->Montage_Stop(0.0f, Montage);
	}

	AnimInst->Montage_Play(Montage);

	UE_LOG(LogMosesCombat, Log, TEXT("[ANIM] %s Play=%s Pawn=%s"),
		DebugTag ? DebugTag : TEXT("Play"),
		*GetNameSafe(Montage),
		*GetNameSafe(this));
}

// ============================================================================
// Fire AV - WeaponData 기반 (Field name unified)
// ============================================================================

void APlayerCharacter::PlayFireAV_Local(FGameplayTag WeaponId) const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!WeaponId.IsValid())
	{
		return;
	}

	if (!WeaponMeshComp || !WeaponMeshComp->GetSkeletalMeshAsset())
	{
		return;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}

	const UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data)
	{
		return;
	}

	const FName MuzzleSocket = Data->MuzzleSocketName;
	if (!MuzzleSocket.IsNone() && !WeaponMeshComp->DoesSocketExist(MuzzleSocket))
	{
		return;
	}

	// 1) Muzzle VFX (Cascade)
	UParticleSystem* MuzzleFX = nullptr;
	if (!Data->MuzzleVFX.IsNull())
	{
		MuzzleFX = Data->MuzzleVFX.Get();
		if (!MuzzleFX)
		{
			MuzzleFX = Data->MuzzleVFX.LoadSynchronous();
		}
	}

	if (MuzzleFX)
	{
		UGameplayStatics::SpawnEmitterAttached(
			MuzzleFX,
			WeaponMeshComp,
			MuzzleSocket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true);
	}

	// 2) Fire SFX
	USoundBase* FireSnd = nullptr;
	if (!Data->FireSFX.IsNull())
	{
		FireSnd = Data->FireSFX.Get();
		if (!FireSnd)
		{
			FireSnd = Data->FireSFX.LoadSynchronous();
		}
	}

	if (FireSnd)
	{
		UGameplayStatics::SpawnSoundAttached(
			FireSnd,
			WeaponMeshComp,
			MuzzleSocket);
	}
}

void APlayerCharacter::ApplyDeadCosmetics_Local() const
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}
}

void APlayerCharacter::Multicast_PlayFireMontage_Implementation(FGameplayTag WeaponId)
{
	TryPlayMontage_Local(FireMontage, TEXT("FIRE"));
	PlayFireAV_Local(WeaponId);
}

void APlayerCharacter::Multicast_PlayHitReactMontage_Implementation()
{
	TryPlayMontage_Local(HitReactMontage, TEXT("HIT"));
}

void APlayerCharacter::Multicast_PlayDeathMontage_Implementation()
{
	TryPlayMontage_Local(DeathMontage, TEXT("DEATH"));
	ApplyDeadCosmetics_Local();
}
