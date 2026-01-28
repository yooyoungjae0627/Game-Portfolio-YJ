#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"
#include "UE5_Multi_Shooter/Pickup/PickupBase.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
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
#include "GameFramework/PlayerController.h"

// ============================================================================
// Local helpers
// ============================================================================

// 회전 정책을 “한 군데”에서 강제 고정
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
		// 이동 방향 회전 금지
		MoveComp->bOrientRotationToMovement = false;

		// (유지) 컨트롤러 Yaw를 Actor가 직접 따라가는 방식 사용
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

	// 입력 바인딩의 주체(로컬 전용)
	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));

	// 카메라 컴포넌트(모드 스택 기반)
	MosesCameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("MosesCameraComponent"));
	MosesCameraComponent->SetupAttachment(GetRootComponent());

	// 코스메틱 무기 메시 컴포넌트(복제 X)
	WeaponMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh());
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComp->SetGenerateOverlapEvents(false);
	WeaponMeshComp->SetIsReplicated(false);

	// 회전 정책 잠금
	Moses_ApplyRotationPolicy(this, TEXT("Ctor"));
}

void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 소켓 부착 보강
	if (WeaponMeshComp && GetMesh())
	{
		WeaponMeshComp->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CharacterWeaponSocketName);

		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][COS] PostInitializeComponents Attach Socket=%s Pawn=%s"),
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
	StopAutoFire_Local(); // [MOD] 종료 시 타이머 정리
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
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero][Input] No HeroComponent. Input will not work. Pawn=%s"), *GetNameSafe(this));
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

	const FRotator YawRot(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MoveValue.Y);
	AddMovementInput(Right, MoveValue.X);
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

	ApplySprintSpeed_LocalPredict(true);
	Server_SetSprinting(true);
}

void APlayerCharacter::Input_SprintReleased()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	ApplySprintSpeed_LocalPredict(false);
	Server_SetSprinting(false);
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

	APickupBase* Target = FindPickupTarget_Local();
	if (Target)
	{
		Server_TryPickup(Target);
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
// [MOD] Hold-to-fire
// ============================================================================

void APlayerCharacter::Input_FirePressed()
{
	if (CachedCombatComponent && CachedCombatComponent->IsDead())
	{
		return;
	}

	// 이미 누르는 중이면 중복 시작 방지
	if (bFireHeldLocal)
	{
		return;
	}

	bFireHeldLocal = true;

	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] FirePressed -> Held=1 Pawn=%s"), *GetNameSafe(this));

	// 1) 누르는 순간 즉시 1발(체감)
	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestFire();
	}

	// 2) 이후 연사 타이머 시작
	StartAutoFire_Local();
}

void APlayerCharacter::Input_FireReleased()
{
	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] FireReleased Pawn=%s"), *GetNameSafe(this));
	StopAutoFire_Local();
}

void APlayerCharacter::StartAutoFire_Local()
{
	// 로컬 컨트롤러만
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire START blocked (NotLocalController) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(AutoFireTimerHandle))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire already active Pawn=%s"), *GetNameSafe(this));
		return;
	}

	// 다음 틱부터 반복 발사
	GetWorldTimerManager().SetTimer(
		AutoFireTimerHandle,
		this,
		&ThisClass::HandleAutoFireTick_Local,
		AutoFireTickRate,
		/*bLoop*/ true,
		/*FirstDelay*/ AutoFireTickRate
	);

	UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][CL] AutoFire START OK Rate=%.3f TimerActive=%d Pawn=%s"),
		AutoFireTickRate,
		GetWorldTimerManager().IsTimerActive(AutoFireTimerHandle) ? 1 : 0,
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
	// 타이머가 돌고 있는지 “증거”로 남김 (Verbose로 낮춰도 됨)
	UE_LOG(LogMosesCombat, VeryVerbose, TEXT("[FIRE][CL] AutoFire TICK Held=%d Pawn=%s"),
		bFireHeldLocal ? 1 : 0,
		*GetNameSafe(this));

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
// Sprint (Server authoritative + Local feel)
// ============================================================================

void APlayerCharacter::ApplySprintSpeed_LocalPredict(bool bNewSprinting)
{
	bLocalPredictedSprinting = bNewSprinting;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bNewSprinting ? SprintSpeed : WalkSpeed;
	}

	UE_LOG(LogMosesMove, Verbose, TEXT("[MOVE][CL] Sprint LocalPredict=%d Pawn=%s"),
		bNewSprinting ? 1 : 0,
		*GetNameSafe(this));
}

void APlayerCharacter::ApplySprintSpeed_FromAuth(const TCHAR* From)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	UE_LOG(LogMosesMove, Log, TEXT("[MOVE][%s] Sprint Auth=%d Pawn=%s"),
		From ? From : TEXT("UNK"),
		bIsSprinting ? 1 : 0,
		*GetNameSafe(this));
}

void APlayerCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Move", TEXT("Client attempted Server_SetSprinting"));

	bIsSprinting = bNewSprinting;
	ApplySprintSpeed_FromAuth(TEXT("SV"));
}

void APlayerCharacter::OnRep_IsSprinting()
{
	ApplySprintSpeed_FromAuth(TEXT("CL"));
}

// ============================================================================
// Pickup (placeholder)
// ============================================================================

APickupBase* APlayerCharacter::FindPickupTarget_Local() const
{
	return nullptr;
}

void APlayerCharacter::Server_TryPickup_Implementation(APickupBase* Target)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Pickup", TEXT("Client attempted Server_TryPickup"));

	UE_LOG(LogMosesCombat, Warning, TEXT("[PICKUP][SV] NotImplemented Target=%s Pawn=%s"),
		*GetNameSafe(Target),
		*GetNameSafe(this));
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

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][Bind] CombatComponent Unbound Pawn=%s"), *GetNameSafe(this));

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
	if (!WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][COS] Skip (Invalid WeaponId) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	if (!WeaponMeshComp)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][COS] Skip (No WeaponMeshComp) Pawn=%s"), *GetNameSafe(this));
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
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][COS] NoRegistry Pawn=%s"), *GetNameSafe(this));
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][COS] ResolveWeaponData FAIL Weapon=%s Pawn=%s"),
			*WeaponId.ToString(),
			*GetNameSafe(this));
		return;
	}

	if (Data->WeaponSkeletalMesh.IsNull())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][COS] NoWeaponMesh(SoftPtr Null) Weapon=%s Pawn=%s"),
			*WeaponId.ToString(),
			*GetNameSafe(this));
		return;
	}

	USkeletalMesh* LoadedMesh = Data->WeaponSkeletalMesh.Get();
	if (!LoadedMesh)
	{
		LoadedMesh = Data->WeaponSkeletalMesh.LoadSynchronous();
	}

	if (!LoadedMesh)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][COS] WeaponMesh Load FAIL Weapon=%s Pawn=%s"),
			*WeaponId.ToString(),
			*GetNameSafe(this));
		return;
	}

	WeaponMeshComp->SetSkeletalMesh(LoadedMesh);

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		WeaponMeshComp->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CharacterWeaponSocketName);
	}

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS] Apply OK Weapon=%s Mesh=%s Socket=%s Pawn=%s"),
		*WeaponId.ToString(),
		*GetNameSafe(LoadedMesh),
		*CharacterWeaponSocketName.ToString(),
		*GetNameSafe(this));
}

// ============================================================================
// Montage Cosmetics (Server approved -> Multicast)
// ============================================================================

void APlayerCharacter::TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const
{
	if (!Montage)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM] %s Montage null Pawn=%s"),
			DebugTag ? DebugTag : TEXT("Montage"),
			*GetNameSafe(this));
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
		UE_LOG(LogMosesCombat, Warning, TEXT("[ANIM] %s NoAnimInstance Pawn=%s"),
			DebugTag ? DebugTag : TEXT("Anim"),
			*GetNameSafe(this));
		return;
	}

	// 클릭/연사 중에도 항상 재생되도록 Restart
	if (AnimInst->Montage_IsPlaying(Montage))
	{
		AnimInst->Montage_Stop(0.0f, Montage);
	}

	AnimInst->Montage_Play(Montage);

	UE_LOG(LogMosesCombat, Log, TEXT("[ANIM] %s PlayMontage(RestartOK)=%s Pawn=%s"),
		DebugTag ? DebugTag : TEXT("Play"),
		*GetNameSafe(Montage),
		*GetNameSafe(this));
}

void APlayerCharacter::PlayFireAV_Local(FGameplayTag WeaponId) const
{
	if (!WeaponMeshComp)
	{
		return;
	}

	if (!WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][AV] Skip (Invalid WeaponId) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}

	const UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][AV] NoRegistry Weapon=%s Pawn=%s"),
			*WeaponId.ToString(), *GetNameSafe(this));
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][AV] ResolveWeaponData FAIL Weapon=%s Pawn=%s"),
			*WeaponId.ToString(), *GetNameSafe(this));
		return;
	}

	const FName MuzzleSocket = Data->MuzzleSocketName.IsNone()
		? FName(TEXT("MuzzleFlash"))
		: Data->MuzzleSocketName;

	const FVector MuzzleLoc = WeaponMeshComp->GetSocketLocation(MuzzleSocket);
	const FRotator MuzzleRot = WeaponMeshComp->GetSocketRotation(MuzzleSocket);

	USoundBase* FireSnd = nullptr;
	if (!Data->FireSound.IsNull())
	{
		FireSnd = Data->FireSound.Get();
		if (!FireSnd)
		{
			FireSnd = Data->FireSound.LoadSynchronous();
		}
	}

	UParticleSystem* MuzzleFX = nullptr;
	if (!Data->MuzzleFlashFX.IsNull())
	{
		MuzzleFX = Data->MuzzleFlashFX.Get();
		if (!MuzzleFX)
		{
			MuzzleFX = Data->MuzzleFlashFX.LoadSynchronous();
		}
	}

	if (FireSnd)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSnd, MuzzleLoc);
	}

	if (MuzzleFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			MuzzleFX,
			FTransform(MuzzleRot, MuzzleLoc),
			true
		);
	}

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][AV] Fire AV OK Weapon=%s Socket=%s SFX=%s VFX=%s Pawn=%s"),
		*WeaponId.ToString(),
		*MuzzleSocket.ToString(),
		*GetNameSafe(FireSnd),
		*GetNameSafe(MuzzleFX),
		*GetNameSafe(this));
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
