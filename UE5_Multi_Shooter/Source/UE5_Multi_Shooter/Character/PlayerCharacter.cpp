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

	// ---------------------------------------------------------------------
	// [DAY6] Weapon mesh components (Hand + Back1/2/3)
	// ---------------------------------------------------------------------
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
	// 실제 소켓이 없으면 에디터에서 바로 확인 가능(로그로도 확인)
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

	// LateJoin/리스폰/레벨 이동에서도 초기 상태를 바로 적용할 수 있도록 한 번 갱신
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

	// PlayerState가 붙는 순간이 "SSOT 접근 가능" 시점이므로 초기 시각화를 보장한다.
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
	CachedCombatComponent->OnReloadingChanged.AddUObject(this, &APlayerCharacter::HandleReloadingChanged);

	// [DAY6] 서버 승인 Swap 컨텍스트 이벤트
	CachedCombatComponent->OnSwapStarted.AddUObject(this, &APlayerCharacter::HandleSwapStarted);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][Bind] CombatComponent Bound Pawn=%s PS=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPlayerState()));

	// 초기 상태를 즉시 적용
	RefreshAllWeaponMeshes_FromSSOT();
	ApplyAttachmentPlan_Immediate(CachedCombatComponent->GetCurrentSlot());

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

	return PS->FindComponentByClass<UMosesCombatComponent>();
}

void APlayerCharacter::HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId)
{
	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS] EquippedChanged Slot=%d Weapon=%s Pawn=%s"),
		SlotIndex,
		*WeaponId.ToString(),
		*GetNameSafe(this));

	// 슬롯/무기 구성은 SSOT를 기준으로 항상 다시 그린다.
	RefreshAllWeaponMeshes_FromSSOT();

	// 스왑 몽타주 중에는 Notify가 Attach 교체를 담당하므로,
	// 중간에 계획을 덮어쓰지 않도록 "스왑 중이 아닐 때만" 즉시 적용한다.
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

void APlayerCharacter::HandleReloadingChanged(bool bReloading)
{
	// 서버 승인 결과(=bIsReloading replicated true)에서만 코스메틱 재생
	if (!bReloading)
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!ReloadMontage)
	{
		UE_LOG(LogMosesWeapon, VeryVerbose, TEXT("[RELOAD][COS] ReloadMontage missing Pawn=%s"), *GetNameSafe(this));
		return;
	}

	TryPlayMontage_Local(ReloadMontage, TEXT("RELOAD"));

	UE_LOG(LogMosesWeapon, Log, TEXT("[RELOAD][COS] ReloadMontage PLAY Pawn=%s"), *GetNameSafe(this));
}

void APlayerCharacter::HandleSwapStarted(int32 FromSlot, int32 ToSlot, int32 Serial)
{
	// 서버 승인 결과로만 들어오는 이벤트다.
	BeginSwapCosmetic_Local(FromSlot, ToSlot, Serial);
}

// ============================================================================
// Weapon visuals (DAY6: Hand + Back1/2/3)
// ============================================================================

void APlayerCharacter::CacheSlotMeshMapping()
{
	// SlotIndex -> "대표 컴포넌트"는 고정한다.
	// 실제 Attach 위치(Hand/Back)는 이벤트 시점에 바뀐다.
	SlotMeshCache[1] = WeaponMesh_Hand;   // Slot1의 "대표"는 HandComp (스왑 중에는 Attach만 이동)
	SlotMeshCache[2] = WeaponMesh_Back1;  // Slot2 대표
	SlotMeshCache[3] = WeaponMesh_Back2;  // Slot3 대표
	SlotMeshCache[4] = WeaponMesh_Back3;  // Slot4 대표

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

FName APlayerCharacter::GetBackSocketNameForSlot(int32 EquippedSlot, int32 SlotIndex) const
{
	// EquippedSlot은 Hand로 가므로, SlotIndex는 Back 중 하나로 매핑되어야 한다.
	// 정책: Equipped 제외 슬롯을 오름차순으로 정렬하여 Back_1/2/3에 순서대로 배치한다.
	TArray<int32> Others;
	Others.Reserve(3);

	for (int32 S = 1; S <= 4; ++S)
	{
		if (S != EquippedSlot)
		{
			Others.Add(S);
		}
	}

	Others.Sort();

	int32 BackIndex = 0;
	for (int32 i = 0; i < Others.Num(); ++i)
	{
		if (Others[i] == SlotIndex)
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
	default: break;
	}

	return WeaponSocket_Back_1;
}

void APlayerCharacter::RefreshAllWeaponMeshes_FromSSOT()
{
	if (!CachedCombatComponent)
	{
		return;
	}

	// SlotMeshCache가 아직 안 잡혔다면 보장
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

	// 무기가 없는 슬롯은 "안 보이게" 처리한다(파밍 전 None 슬롯)
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
	if (!GetMesh())
	{
		return;
	}

	if (!CachedCombatComponent)
	{
		return;
	}

	EquippedSlot = FMath::Clamp(EquippedSlot, 1, 4);

	// EquippedSlot -> Hand, Others -> Back sockets
	for (int32 Slot = 1; Slot <= 4; ++Slot)
	{
		USkeletalMeshComponent* Comp = GetMeshCompForSlot(Slot);
		if (!Comp)
		{
			continue;
		}

		// 무기 없는 슬롯이면 attach는 의미 없지만, 상태 일관성을 위해 붙여도 무방
		const FName SocketName = (Slot == EquippedSlot) ? GetHandSocketName() : GetBackSocketNameForSlot(EquippedSlot, Slot);

		Comp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS] ApplyAttachmentPlan EquippedSlot=%d Pawn=%s"),
		EquippedSlot,
		*GetNameSafe(this));
}

// ============================================================================
// [DAY6] Swap runtime (Cosmetic only)
// ============================================================================

void APlayerCharacter::BeginSwapCosmetic_Local(int32 FromSlot, int32 ToSlot, int32 Serial)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FromSlot = FMath::Clamp(FromSlot, 1, 4);
	ToSlot = FMath::Clamp(ToSlot, 1, 4);

	// 방어: 같은 슬롯이면 스왑 연출 필요 없음
	if (FromSlot == ToSlot)
	{
		return;
	}

	// 이전 스왑이 남아있으면 강제 종료(예: 짧은 입력 연속)
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

	// Swap 도중에도 무기 메쉬는 SSOT 기준으로 항상 최신이어야 한다.
	RefreshAllWeaponMeshes_FromSSOT();

	// 몽타주 재생(서버 승인 결과에서만 호출됨)
	if (!SwapMontage)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS] SwapMontage missing -> Fallback ApplyAttachmentPlan Pawn=%s"), *GetNameSafe(this));
		// 몽타주가 없으면 즉시 Attach 계획만 적용하고 종료
		ApplyAttachmentPlan_Immediate(ToSlot);
		EndSwapCosmetic_Local(TEXT("NoMontageFallback"));
		return;
	}

	TryPlayMontage_Local(SwapMontage, TEXT("SWAP"));
}

void APlayerCharacter::EndSwapCosmetic_Local(const TCHAR* Reason)
{
	// 스왑이 끝나면 "정답 상태"를 SSOT 기준으로 한 번 더 강제한다.
	// Notify가 누락되거나 몽타주가 끊겨도 결과가 일치해야 한다.
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

	if (!bSwapInProgress)
	{
		return;
	}

	if (bSwapDetachDone)
	{
		return;
	}

	bSwapDetachDone = true;

	// "기존 손 무기"를 등으로 보낸다.
	// FromSlot이 등에서 어느 위치로 가야 하는지는 "스왑 완료 후" 기준(ToSlot equipped)으로 결정한다.
	const int32 FromSlot = PendingSwapFromSlot;
	const int32 ToSlot = PendingSwapToSlot;

	USkeletalMeshComponent* FromComp = GetMeshCompForSlot(FromSlot);
	if (!FromComp || !GetMesh())
	{
		return;
	}

	const FName BackSocket = GetBackSocketNameForSlot(ToSlot, FromSlot);
	FromComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackSocket);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][COS][DETACH] FromSlot=%d -> Socket=%s Pawn=%s"),
		FromSlot,
		*BackSocket.ToString(),
		*GetNameSafe(this));
}

void APlayerCharacter::HandleSwapAttachNotify()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bSwapInProgress)
	{
		return;
	}

	if (bSwapAttachDone)
	{
		return;
	}

	bSwapAttachDone = true;

	// "새 무기"를 손에 붙인다.
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

	// Attach까지 끝났으면 "정답 상태"로 한 번 더 맞춘 후 종료
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

	// 현재 손에 붙은 무기가 발사 연출의 기준이다.
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
