#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h"

#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesHeroComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Characters/Animation/MosesAnimInstance.h"
#include "UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h"
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

static void Moses_ConfigWeaponMeshComp(USkeletalMeshComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetIsReplicated(false);
	Comp->SetCastShadow(true);
	Comp->bReceivesDecals = false;
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

	InteractionComponent = CreateDefaultSubobject<UMosesInteractionComponent>(TEXT("InteractionComponent"));

	WeaponMesh_Hand = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh_Hand"));
	WeaponMesh_Hand->SetupAttachment(GetMesh());
	Moses_ConfigWeaponMeshComp(WeaponMesh_Hand);

	WeaponMesh_Back1 = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh_Back1"));
	WeaponMesh_Back1->SetupAttachment(GetMesh());
	Moses_ConfigWeaponMeshComp(WeaponMesh_Back1);

	WeaponMesh_Back2 = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh_Back2"));
	WeaponMesh_Back2->SetupAttachment(GetMesh());
	Moses_ConfigWeaponMeshComp(WeaponMesh_Back2);

	WeaponMesh_Back3 = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh_Back3"));
	WeaponMesh_Back3->SetupAttachment(GetMesh());
	Moses_ConfigWeaponMeshComp(WeaponMesh_Back3);

	Moses_ApplyRotationPolicy(this, TEXT("Ctor"));
}

void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 초기 Attach는 "일단" 소켓 존재 여부와 무관하게 실행한다.
	if (GetMesh())
	{
		if (WeaponMesh_Hand)
		{
			WeaponMesh_Hand->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetHandSocketName());
		}
		if (WeaponMesh_Back1)
		{
			WeaponMesh_Back1->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocket_Back_1);
		}
		if (WeaponMesh_Back2)
		{
			WeaponMesh_Back2->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocket_Back_2);
		}
		if (WeaponMesh_Back3)
		{
			WeaponMesh_Back3->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocket_Back_3);
		}
	}

	CacheSlotMeshMapping();

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

	RefreshAllWeaponMeshes_FromSSOT();
	if (CachedCombatComponent)
	{
		ApplyAttachmentPlan_Immediate(CachedCombatComponent->GetCurrentSlot());
	}
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

	RefreshAllWeaponMeshes_FromSSOT();
	if (CachedCombatComponent)
	{
		ApplyAttachmentPlan_Immediate(CachedCombatComponent->GetCurrentSlot());
	}
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

void APlayerCharacter::Input_LookYaw(float Value)
{
	AddControllerYawInput(Value);
}

void APlayerCharacter::Input_LookPitch(float Value)
{
	AddControllerPitchInput(Value);
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

void APlayerCharacter::Input_EquipSlot4()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(4);
	}
}

void APlayerCharacter::Input_Reload()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		// (증거 로그용) R 입력이 실제로 Combat으로 들어가는지 확인
		UE_LOG(LogMosesWeapon, Warning, TEXT("[RELOAD][CL][INPUT] R Press -> RequestReload Pawn=%s"), *GetNameSafe(this));

		CombatComp->RequestReload();
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
	// ✅ [MOD] bFireHeldLocal 여부와 무관하게 "무조건" 타이머 끊기
	bFireHeldLocal = false;
	GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);

	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire STOP OK Pawn=%s"), *GetNameSafe(this));
}

void APlayerCharacter::HandleAutoFireTick_Local()
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		StopAutoFire_Local();
		return;
	}

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
	CachedCombatComponent->OnReloadingChanged.AddUObject(this, &APlayerCharacter::HandleReloadingChanged);

	CachedCombatComponent->OnSwapStarted.AddUObject(this, &APlayerCharacter::HandleSwapStarted);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][Bind] CombatComponent Bound Pawn=%s PS=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPlayerState()));

	RefreshAllWeaponMeshes_FromSSOT();
	ApplyAttachmentPlan_Immediate(CachedCombatComponent->GetCurrentSlot());

	HandleDeadChanged(CachedCombatComponent->IsDead());

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][Bind][DBG] Pawn=%s BoundCombat=%s PS=%s PS_Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(CachedCombatComponent),
		*GetNameSafe(GetPlayerState()),
		*GetNameSafe(GetPlayerState() ? GetPlayerState()->GetPawn() : nullptr));

}

void APlayerCharacter::UnbindCombatComponent()
{
	if (!CachedCombatComponent)
	{
		return;
	}

	CachedCombatComponent->OnEquippedChanged.RemoveAll(this);
	CachedCombatComponent->OnDeadChanged.RemoveAll(this);
	CachedCombatComponent->OnReloadingChanged.RemoveAll(this);
	CachedCombatComponent->OnSwapStarted.RemoveAll(this);

	CachedCombatComponent = nullptr;
}

UMosesCombatComponent* APlayerCharacter::GetCombatComponent_Checked() const
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return nullptr;
	}

	// ✅ [FIX] FindComponentByClass 금지. SSOT 포인터 직접 접근.
	return PS->GetCombatComponent();
}

void APlayerCharacter::HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId)
{
	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS] EquippedChanged Slot=%d Weapon=%s Pawn=%s"),
		SlotIndex,
		*WeaponId.ToString(),
		*GetNameSafe(this));

	RefreshAllWeaponMeshes_FromSSOT();

	// 스왑 몽타주 중에는 Notify가 Attach 교체를 담당하므로 중간 덮어쓰기 금지.
	if (!bSwapInProgress && CachedCombatComponent)
	{
		ApplyAttachmentPlan_Immediate(CachedCombatComponent->GetCurrentSlot());
	}
}

void APlayerCharacter::HandleDeadChanged(bool bNewDead)
{
	UE_LOG(LogMosesCombat, Log, TEXT("[DEAD][COS] DeadChanged bDead=%d Pawn=%s"),
		bNewDead ? 1 : 0,
		*GetNameSafe(this));

	// AnimBP 분기용 Dead 상태 전달 (Tick 금지: 상태 변경 시 1회)
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			if (UMosesAnimInstance* MosesAnim = Cast<UMosesAnimInstance>(AnimInst))
			{
				MosesAnim->SetIsDead(bNewDead);
			}
		}
	}

	// ---------------------------------------------------------------------
	// Dead 해제(리스폰): 입력/이동/몽타주 정리
	// ---------------------------------------------------------------------
	if (!bNewDead)
	{
		StopAutoFire_Local();

		if (APlayerController* PC = GetController<APlayerController>())
		{
			if (PC->IsLocalController())
			{
				PC->SetIgnoreMoveInput(false);
				PC->SetIgnoreLookInput(false);
			}
		}

		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->SetMovementMode(EMovementMode::MOVE_Walking);
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			if (USkeletalMeshComponent* MeshComp = GetMesh())
			{
				if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
				{
					// ✅ 리스폰 시에는 전체 정리 OK
					AnimInst->Montage_Stop(0.10f);
				}
			}
		}

		UE_LOG(LogMosesCombat, Log, TEXT("[DEAD][COS] Cleared -> Input/Move restored Pawn=%s"), *GetNameSafe(this));
		return;
	}

	// ---------------------------------------------------------------------
	// Dead 진입: 입력/오토파이어/이동 정지 + (중요) DeathMontage는 절대 끊지 않는다
	// ---------------------------------------------------------------------
	StopAutoFire_Local();

	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (PC->IsLocalController())
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
	}

	// ✅ [FIX] 전체 Montage_Stop(0.0f) 금지
	// - RepNotify(DeadChanged)가 Multicast(DeathMontage)보다 늦게 오면
	//   여기서 DeathMontage까지 같이 끊겨서 “재생 로그는 찍히는데 화면에서 안 보이는” 현상이 난다.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				// DeathMontage는 보호하고, 나머지만 끊는다.
				if (FireMontage && AnimInst->Montage_IsPlaying(FireMontage))
				{
					AnimInst->Montage_Stop(0.0f, FireMontage);
				}
				if (ReloadMontage && AnimInst->Montage_IsPlaying(ReloadMontage))
				{
					AnimInst->Montage_Stop(0.0f, ReloadMontage);
				}
				if (SwapMontage && AnimInst->Montage_IsPlaying(SwapMontage))
				{
					AnimInst->Montage_Stop(0.0f, SwapMontage);
				}
				if (HitReactMontage && AnimInst->Montage_IsPlaying(HitReactMontage))
				{
					AnimInst->Montage_Stop(0.0f, HitReactMontage);
				}

				// ✅ 혹시 DeathMontage가 이미 재생 중이면 절대 Stop 금지
				if (DeathMontage && AnimInst->Montage_IsPlaying(DeathMontage))
				{
					UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][COS] DeathMontage already playing -> KEEP Pawn=%s"), *GetNameSafe(this));
				}
			}
		}
	}

	ApplyDeadCosmetics_Local();

	UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][COS] Locked input + stopped non-death montages Pawn=%s"), *GetNameSafe(this));

	// DeathMontage는 서버 Multicast에서만 재생 (정책 유지)
}

void APlayerCharacter::HandleReloadingChanged(bool bReloading)
{
	// 리로드 시작(true)일 때만 몽타주 재생
	if (!bReloading)
	{
		return;
	}

	// Dedicated Server는 코스메틱 금지
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 몽타주 에셋 없으면 종료
	if (!ReloadMontage)
	{
		UE_LOG(LogMosesWeapon, VeryVerbose, TEXT("[RELOAD][COS] ReloadMontage missing Pawn=%s"), *GetNameSafe(this));
		return;
	}

	// Swap 몽타주 중이면(원하면) 리로드 재생을 막아도 됨
	// (현재는 정책 선택사항. 충돌이 보이면 아래 가드 ON 권장)
	// if (bSwapInProgress)
	// {
	// 	UE_LOG(LogMosesWeapon, Verbose, TEXT("[RELOAD][COS] SKIP (SwapInProgress) Pawn=%s"), *GetNameSafe(this));
	// 	return;
	// }

	// ✅ 상체 슬롯(UpperBody)은 ReloadMontage의 Slot Track이 담당해야 함
	TryPlayMontage_Local(ReloadMontage, TEXT("RELOAD"));

	UE_LOG(LogMosesWeapon, Log, TEXT("[RELOAD][COS] ReloadMontage PLAY Pawn=%s"), *GetNameSafe(this));
}

void APlayerCharacter::HandleSwapStarted(int32 FromSlot, int32 ToSlot, int32 Serial)
{
	BeginSwapCosmetic_Local(FromSlot, ToSlot, Serial);
}

// ============================================================================
// Weapon visuals
// ============================================================================

void APlayerCharacter::CacheSlotMeshMapping()
{
	SlotMeshCache[1] = WeaponMesh_Hand;
	SlotMeshCache[2] = WeaponMesh_Back1;
	SlotMeshCache[3] = WeaponMesh_Back2;
	SlotMeshCache[4] = WeaponMesh_Back3;

	bSlotMeshCacheBuilt = true;

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][COS] SlotMeshCache built Pawn=%s"), *GetNameSafe(this));
}

USkeletalMeshComponent* APlayerCharacter::GetMeshCompForSlot(int32 SlotIndex) const
{
	if (!bSlotMeshCacheBuilt)
	{
		return nullptr;
	}

	if (SlotIndex < 1 || SlotIndex > 4)
	{
		return nullptr;
	}

	return SlotMeshCache[SlotIndex];
}

FName APlayerCharacter::GetHandSocketName() const
{
	return WeaponSocket_Hand;
}

void APlayerCharacter::BuildBackSlotList_UsingSSOT(int32 EquippedSlot, TArray<int32>& OutBackSlots) const
{
	OutBackSlots.Reset();

	EquippedSlot = FMath::Clamp(EquippedSlot, 1, 4);

	// [STEP2] "무기 있는 슬롯"만 모아서 Back_1~3을 빈칸 없이 채운다.
	if (!CachedCombatComponent)
	{
		return;
	}

	for (int32 Slot = 1; Slot <= 4; ++Slot)
	{
		if (Slot == EquippedSlot)
		{
			continue;
		}

		const FGameplayTag WeaponId = CachedCombatComponent->GetWeaponIdForSlot(Slot);
		if (WeaponId.IsValid())
		{
			OutBackSlots.Add(Slot);
		}
	}

	OutBackSlots.Sort();
}

FName APlayerCharacter::GetBackSocketNameForSlot(int32 EquippedSlot, int32 SlotIndex) const
{
	EquippedSlot = FMath::Clamp(EquippedSlot, 1, 4);
	SlotIndex = FMath::Clamp(SlotIndex, 1, 4);

	// Hand는 여기서 다루지 않는다.
	if (SlotIndex == EquippedSlot)
	{
		return WeaponSocket_Back_1;
	}

	TArray<int32> BackSlots;
	BuildBackSlotList_UsingSSOT(EquippedSlot, BackSlots);

	int32 BackIndex = INDEX_NONE;
	for (int32 i = 0; i < BackSlots.Num(); ++i)
	{
		if (BackSlots[i] == SlotIndex)
		{
			BackIndex = i; // 0..2
			break;
		}
	}

	switch (BackIndex)
	{
	case 0: return WeaponSocket_Back_1;
	case 1: return WeaponSocket_Back_2;
	case 2: return WeaponSocket_Back_3;
	default:
		// 무기 없는 슬롯(또는 데이터 불일치)은 안전한 소켓으로 폴백.
		return WeaponSocket_Back_3;
	}
}

void APlayerCharacter::RefreshAllWeaponMeshes_FromSSOT()
{
	if (!CachedCombatComponent)
	{
		return;
	}

	if (!bSlotMeshCacheBuilt)
	{
		CacheSlotMeshMapping();
	}

	for (int32 Slot = 1; Slot <= 4; ++Slot)
	{
		const FGameplayTag WeaponId = CachedCombatComponent->GetWeaponIdForSlot(Slot);
		USkeletalMeshComponent* TargetComp = GetMeshCompForSlot(Slot);
		RefreshWeaponMesh_ForSlot(Slot, WeaponId, TargetComp);
	}
}

void APlayerCharacter::RefreshWeaponMesh_ForSlot(int32 SlotIndex, FGameplayTag WeaponId, USkeletalMeshComponent* TargetMeshComp)
{
	if (!TargetMeshComp)
	{
		return;
	}

	if (!WeaponId.IsValid())
	{
		TargetMeshComp->SetSkeletalMesh(nullptr);
		TargetMeshComp->SetVisibility(false, true);
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
		TargetMeshComp->SetSkeletalMesh(nullptr);
		TargetMeshComp->SetVisibility(false, true);
		return;
	}

	USkeletalMesh* LoadedMesh = Data->WeaponSkeletalMesh.Get();
	if (!LoadedMesh)
	{
		LoadedMesh = Data->WeaponSkeletalMesh.LoadSynchronous();
	}

	if (!LoadedMesh)
	{
		TargetMeshComp->SetSkeletalMesh(nullptr);
		TargetMeshComp->SetVisibility(false, true);
		return;
	}

	TargetMeshComp->SetSkeletalMesh(LoadedMesh);
	TargetMeshComp->SetVisibility(true, true);

	UE_LOG(LogMosesWeapon, VeryVerbose, TEXT("[WEAPON][COS] MeshSet Slot=%d Weapon=%s Mesh=%s Pawn=%s"),
		SlotIndex,
		*WeaponId.ToString(),
		*GetNameSafe(LoadedMesh),
		*GetNameSafe(this));
}

void APlayerCharacter::ApplyAttachmentPlan_Immediate(int32 EquippedSlot)
{
	if (!GetMesh() || !CachedCombatComponent)
	{
		return;
	}

	EquippedSlot = FMath::Clamp(EquippedSlot, 1, 4);

	// [STEP2] Back list는 "무기 있는 슬롯만"으로 압축된다.
	TArray<int32> BackSlots;
	BuildBackSlotList_UsingSSOT(EquippedSlot, BackSlots);

	for (int32 Slot = 1; Slot <= 4; ++Slot)
	{
		USkeletalMeshComponent* Comp = GetMeshCompForSlot(Slot);
		if (!Comp)
		{
			continue;
		}

		const FGameplayTag WeaponId = CachedCombatComponent->GetWeaponIdForSlot(Slot);
		const bool bHasWeapon = WeaponId.IsValid();

		// 손 슬롯은 무기 유무와 관계없이 "정답 소켓"에 붙인다.
		if (Slot == EquippedSlot)
		{
			Comp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetHandSocketName());
			continue;
		}

		// 등 슬롯은 "무기 있는 슬롯"만 빈칸 없이 배치한다.
		if (bHasWeapon)
		{
			const FName BackSocket = GetBackSocketNameForSlot(EquippedSlot, Slot);
			Comp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackSocket);
		}
		else
		{
			// 무기 없는 슬롯은 숨김 상태이므로, 안전한 소켓으로 폴백 (시각 영향 없음)
			Comp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocket_Back_3);
		}
	}

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS][STEP2] ApplyAttachmentPlan EquippedSlot=%d BackSlots=%d Pawn=%s"),
		EquippedSlot,
		BackSlots.Num(),
		*GetNameSafe(this));
}

// ============================================================================
// Swap runtime (Cosmetic only)
// ============================================================================

void APlayerCharacter::BeginSwapCosmetic_Local(int32 FromSlot, int32 ToSlot, int32 Serial)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FromSlot = FMath::Clamp(FromSlot, 1, 4);
	ToSlot = FMath::Clamp(ToSlot, 1, 4);

	if (FromSlot == ToSlot)
	{
		return;
	}

	if (bSwapInProgress)
	{
		EndSwapCosmetic_Local(TEXT("ForceEnd(Overlapped)"));
	}

	bSwapInProgress = true;
	PendingSwapFromSlot = FromSlot;
	PendingSwapToSlot = ToSlot;
	PendingSwapSerial = Serial;

	bSwapDetachDone = false;
	bSwapAttachDone = false;

	UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS] BeginSwap From=%d To=%d Serial=%d Pawn=%s"),
		FromSlot, ToSlot, Serial, *GetNameSafe(this));

	RefreshAllWeaponMeshes_FromSSOT();

	if (!SwapMontage)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS] SwapMontage missing -> Fallback ApplyAttachmentPlan Pawn=%s"), *GetNameSafe(this));

		ApplyAttachmentPlan_Immediate(ToSlot);
		EndSwapCosmetic_Local(TEXT("NoMontageFallback"));
		return;
	}

	TryPlayMontage_Local(SwapMontage, TEXT("SWAP"));
}

void APlayerCharacter::EndSwapCosmetic_Local(const TCHAR* Reason)
{
	if (CachedCombatComponent)
	{
		ApplyAttachmentPlan_Immediate(CachedCombatComponent->GetCurrentSlot());
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS] EndSwap Reason=%s Detach=%d Attach=%d Pawn=%s"),
		Reason ? Reason : TEXT("None"),
		bSwapDetachDone ? 1 : 0,
		bSwapAttachDone ? 1 : 0,
		*GetNameSafe(this));

	bSwapInProgress = false;
	bSwapDetachDone = false;
	bSwapAttachDone = false;
}

void APlayerCharacter::HandleSwapDetachNotify()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bSwapInProgress || bSwapDetachDone)
	{
		return;
	}

	bSwapDetachDone = true;

	const int32 FromSlot = PendingSwapFromSlot;
	const int32 ToSlot = PendingSwapToSlot;

	USkeletalMeshComponent* FromComp = GetMeshCompForSlot(FromSlot);
	if (!FromComp || !GetMesh())
	{
		return;
	}

	// [STEP2] FromSlot을 "최종 상태 기준(ToSlot equipped)"으로 Back_1~3에 빈칸 없이 재배치
	const FName BackSocket = GetBackSocketNameForSlot(ToSlot, FromSlot);
	FromComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackSocket);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS][DETACH][STEP2] FromSlot=%d -> Socket=%s (Equipped=%d) Pawn=%s"),
		FromSlot,
		*BackSocket.ToString(),
		ToSlot,
		*GetNameSafe(this));
}

void APlayerCharacter::HandleSwapAttachNotify()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bSwapInProgress || bSwapAttachDone)
	{
		return;
	}

	bSwapAttachDone = true;

	const int32 ToSlot = PendingSwapToSlot;

	USkeletalMeshComponent* ToComp = GetMeshCompForSlot(ToSlot);
	if (!ToComp || !GetMesh())
	{
		return;
	}

	ToComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetHandSocketName());

	UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS][ATTACH] ToSlot=%d -> HandSocket=%s Pawn=%s"),
		ToSlot,
		*GetHandSocketName().ToString(),
		*GetNameSafe(this));

	EndSwapCosmetic_Local(TEXT("NotifyAttachDone"));
}

// ============================================================================
// Montage Cosmetics
// ============================================================================

void APlayerCharacter::TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// [MOD] SSOT Dead 상태면 어떤 몽타주도 재생 금지(DeathMontage만 예외적으로 허용)
	const bool bIsDeadSSOT = (CachedCombatComponent && CachedCombatComponent->IsDead());
	if (bIsDeadSSOT)
	{
		// DeathMontage 자체는 허용 (Multicast_PlayDeathMontage에서만 재생)
		if (Montage != DeathMontage)
		{
			UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][%s] SKIP (Dead) Montage=%s Pawn=%s"),
				DebugTag ? DebugTag : TEXT("UNK"),
				*GetNameSafe(Montage),
				*GetNameSafe(this));
			return;
		}
	}

	if (!Montage)
	{
		UE_LOG(LogMosesCombat, Error, TEXT("[ANIM][%s] FAIL Montage=NULL Pawn=%s"),
			DebugTag ? DebugTag : TEXT("UNK"),
			*GetNameSafe(this));
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogMosesCombat, Error, TEXT("[ANIM][%s] FAIL Mesh=NULL Pawn=%s"),
			DebugTag ? DebugTag : TEXT("UNK"),
			*GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogMosesCombat, Error, TEXT("[ANIM][%s] FAIL AnimInstance=NULL Mesh=%s Pawn=%s AnimClass=%s"),
			DebugTag ? DebugTag : TEXT("UNK"),
			*GetNameSafe(MeshComp),
			*GetNameSafe(this),
			*GetNameSafe(MeshComp->AnimClass));
		return;
	}

	// [MOD] DeathMontage가 재생 중이면 다른 몽타주(특히 FIRE/RELOAD)가 덮어쓰지 못하게 차단
	if (DeathMontage && AnimInst->Montage_IsPlaying(DeathMontage) && Montage != DeathMontage)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][%s] SKIP (DeathPlaying) Req=%s Death=%s Pawn=%s"),
			DebugTag ? DebugTag : TEXT("UNK"),
			*GetNameSafe(Montage),
			*GetNameSafe(DeathMontage),
			*GetNameSafe(this));
		return;
	}

	UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][%s] PLAY Montage=%s AnimInst=%s Pawn=%s"),
		DebugTag ? DebugTag : TEXT("UNK"),
		*GetNameSafe(Montage),
		*GetNameSafe(AnimInst),
		*GetNameSafe(this));

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
// Fire AV - WeaponData 기반 (현재 장착 무기 = Hand mesh 기준)
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

	USkeletalMeshComponent* HandComp = WeaponMesh_Hand;
	if (!HandComp || !HandComp->GetSkeletalMeshAsset())
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
	if (!MuzzleSocket.IsNone() && !HandComp->DoesSocketExist(MuzzleSocket))
	{
		return;
	}

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
			HandComp,
			MuzzleSocket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true);
	}

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
		UGameplayStatics::SpawnSoundAttached(FireSnd, HandComp, MuzzleSocket);
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
	// Dedicated Server는 코스메틱 금지
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// [MOD] SSOT Dead면 Fire 코스메틱 절대 재생 금지
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][FIRE] SKIP (Dead) Pawn=%s"), *GetNameSafe(this));
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

	// [MOD] DeathMontage가 재생 중이면 Fire가 덮어쓰지 못하게 차단
	if (DeathMontage && AnimInst->Montage_IsPlaying(DeathMontage))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][FIRE] SKIP (DeathPlaying) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	TryPlayMontage_Local(FireMontage, TEXT("FIRE"));
	PlayFireAV_Local(WeaponId);
}


void APlayerCharacter::Multicast_PlayHitReactMontage_Implementation()
{
	TryPlayMontage_Local(HitReactMontage, TEXT("HIT"));
}

void APlayerCharacter::Multicast_PlayDeathMontage_Implementation()
{
	UE_LOG(LogMosesCombat, Warning,
		TEXT("[ANIM][DEATH][MC] THIS=%s NetMode=%d World=%s"),
		*GetNameSafe(this),
		(int32)GetNetMode(),
		*GetNameSafe(GetWorld()));


	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!DeathMontage)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][DEATH] SKIP (NoDeathMontage) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][DEATH] SKIP (NoMesh) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][DEATH] SKIP (NoAnimInst) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	// ✅ 죽음은 최우선: 모든 몽타주 즉시 정지
	AnimInst->Montage_Stop(0.0f);

	AnimInst->Montage_Play(DeathMontage);

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[ANIM][DEATH] PLAY Montage=%s AnimInst=%s Pawn=%s"),
		*GetNameSafe(DeathMontage),
		*GetNameSafe(AnimInst),
		*GetNameSafe(this));

	// 이동 정지(코스메틱)
	ApplyDeadCosmetics_Local();

	UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM][DEATH][DBG] Pawn=%s NetMode=%d World=%s"),
		*GetNameSafe(this), (int32)GetNetMode(), *GetNameSafe(GetWorld()));
}

void APlayerCharacter::Multicast_PlayDeathMontage_WithPid_Implementation(const FString& VictimPid)
{
	if (GetNetMode() == NM_DedicatedServer) return;

	const AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	const FString MyPid = PS ? PS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens) : TEXT("");

	if (!VictimPid.IsEmpty() && MyPid != VictimPid)
	{
		UE_LOG(LogMosesCombat, Verbose, TEXT("[ANIM][DEATH] SKIP (PidMismatch) Req=%s My=%s Pawn=%s"),
			*VictimPid, *MyPid, *GetNameSafe(this));
		return;
	}

	// 기존 Multicast_PlayDeathMontage 내용 그대로 수행
	Multicast_PlayDeathMontage_Implementation();
}
