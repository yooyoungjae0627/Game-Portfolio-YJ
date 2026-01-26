#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"
#include "UE5_Multi_Shooter/Pickup/PickupBase.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// ============================================================================
// Ctor / Lifecycle
// ============================================================================

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 코스메틱 무기 메시 컴포넌트는 "항상 존재"하게 보장(늦게 생성되면 Attach 타이밍 꼬임)
	EnsureWeaponMeshComponent();
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 이동 기본 속도 적용(로컬/서버 공통)
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}

	// PlayerState/CombatComponent 바인딩 보강
	BindCombatComponent();
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCombatComponent();
	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 프로젝트 정책: EnhancedInput 바인딩은 HeroComponent가 담당.
	// Pawn은 입력 엔드포인트 함수(Input_*)만 제공한다.
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
}

// ============================================================================
// Possession / Restart hooks (바인딩 안정화)
// ============================================================================

void APlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	BindCombatComponent();
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	BindCombatComponent();
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
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
// Input Endpoints (HeroComponent -> Pawn)
// ============================================================================

void APlayerCharacter::Input_Move(const FVector2D& MoveValue)
{
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
	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void APlayerCharacter::Input_SprintPressed()
{
	// 체감(로컬): 즉시 속도 반영
	ApplySprintSpeed_LocalPredict(true);

	// 정답(서버): 서버가 승인하고 복제로 내려준다
	Server_SetSprinting(true);
}

void APlayerCharacter::Input_SprintReleased()
{
	ApplySprintSpeed_LocalPredict(false);
	Server_SetSprinting(false);
}

void APlayerCharacter::Input_JumpPressed()
{
	Jump();
}

void APlayerCharacter::Input_InteractPressed()
{
	// 로컬 타겟 탐색 -> 서버 TryPickup 요청
	APickupBase* Target = FindPickupTarget_Local();
	if (Target)
	{
		Server_TryPickup(Target);
	}
}

void APlayerCharacter::Input_EquipSlot1()
{
	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(1);
	}
}

void APlayerCharacter::Input_EquipSlot2()
{
	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(2);
	}
}

void APlayerCharacter::Input_EquipSlot3()
{
	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		CombatComp->RequestEquipSlot(3);
	}
}

void APlayerCharacter::Input_FirePressed()
{
	// - 클라 입력은 '요청'만 한다.
	// - 서버 승인/탄약 차감/명중 판정/데미지 적용은 CombatComponent(서버)에서 수행한다.
	// - 서버가 승인한 뒤에만 Multicast_PlayFireMontage()로 코스메틱 재생을 퍼뜨린다.
	if (UMosesCombatComponent* CombatComp = GetCombatComponent_Checked())
	{
		UE_LOG(LogMosesCombat, Verbose, TEXT("[FIRE][CL] Input FirePressed Pawn=%s"), *GetNameSafe(this));
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

	// 서버만 정답을 변경한다
	bIsSprinting = bNewSprinting;

	// 서버(리슨 포함)는 즉시 반영
	ApplySprintSpeed_FromAuth(TEXT("SV"));
}

void APlayerCharacter::OnRep_IsSprinting()
{
	// 클라이언트는 서버 확정값으로 최종 보정한다
	ApplySprintSpeed_FromAuth(TEXT("CL"));
}

// ============================================================================
// Pickup (placeholder)
// ============================================================================

APickupBase* APlayerCharacter::FindPickupTarget_Local() const
{
	// TODO(Day2): 프로젝트에 이미 구현된 Trace/Overlap 기반 타겟 탐색이 있다면 그 코드로 교체.
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

	// 이미 같은 컴포넌트에 바인딩되어 있으면 중복 방지
	if (CachedCombatComponent == NewComp)
	{
		return;
	}

	UnbindCombatComponent();

	CachedCombatComponent = NewComp;

	// CombatComponent 쪽에서 RepNotify가 발생하면 Delegate로 알려주고,
	// Pawn은 그 Delegate를 받아서 코스메틱(무기 메시)을 갱신한다.
	CachedCombatComponent->OnEquippedChanged.AddUObject(this, &APlayerCharacter::HandleEquippedChanged);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][Bind] CombatComponent Bound Pawn=%s PS=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPlayerState()));

	// 늦게 바인딩되는 케이스(예: Late Join / ClientRestart) 대비:
	// 바인딩 직후 현재 상태를 한 번 "즉시 재현"한다.
	HandleEquippedChanged(CachedCombatComponent->GetCurrentSlot(), CachedCombatComponent->GetEquippedWeaponId());
}

void APlayerCharacter::UnbindCombatComponent()
{
	if (!CachedCombatComponent)
	{
		return;
	}

	CachedCombatComponent->OnEquippedChanged.RemoveAll(this);

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

	// 정책: CombatComponent는 PlayerState가 SSOT로 소유한다.
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

void APlayerCharacter::RefreshWeaponCosmetic(FGameplayTag WeaponId)
{
	if (!WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][COS] Skip (Invalid WeaponId) Pawn=%s"), *GetNameSafe(this));
		return;
	}

	EnsureWeaponMeshComponent();

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

	if (!Data->WeaponSkeletalMesh)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][COS] NoWeaponMesh Weapon=%s Pawn=%s"),
			*WeaponId.ToString(),
			*GetNameSafe(this));
		return;
	}

	// TSoftObjectPtr 로드
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

	// 메시 교체
	WeaponMeshComp->SetSkeletalMesh(LoadedMesh);

	// 안전: 혹시라도 소켓 부착이 풀렸으면 재부착(에디터/PIE 재시작 등)
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

void APlayerCharacter::EnsureWeaponMeshComponent()
{
	if (WeaponMeshComp)
	{
		return;
	}

	// 코스메틱 전용 무기 메시 컴포넌트 (Replication X)
	WeaponMeshComp = NewObject<USkeletalMeshComponent>(this, TEXT("WeaponMeshComp"));
	check(WeaponMeshComp);

	WeaponMeshComp->SetIsReplicated(false);

	// 캐릭터 메시의 손 소켓에 "스냅" 부착 (생성 즉시 부착해두고, 이후에는 Mesh만 교체)
	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		WeaponMeshComp->SetupAttachment(CharMesh);
	}
	else
	{
		// Mesh가 아직 없을 수도 있으니 일단 Root에 달아두고,
		// RefreshWeaponCosmetic에서 다시 AttachToComponent로 보정한다.
		WeaponMeshComp->SetupAttachment(GetRootComponent());
	}

	WeaponMeshComp->RegisterComponent();

	// 부착 보강(가능한 경우)
	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		WeaponMeshComp->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CharacterWeaponSocketName);
	}

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][COS] WeaponMeshComp Created Pawn=%s Socket=%s"),
		*GetNameSafe(this),
		*CharacterWeaponSocketName.ToString());
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

	AnimInst->Montage_Play(Montage);

	UE_LOG(LogMosesCombat, Log, TEXT("[ANIM] %s PlayMontage=%s Pawn=%s"),
		DebugTag ? DebugTag : TEXT("Play"),
		*GetNameSafe(Montage),
		*GetNameSafe(this));
}

void APlayerCharacter::Multicast_PlayFireMontage_Implementation()
{
	// 서버 승인 후에만 호출되어야 한다(CombatComponent 서버 로직에서 호출)
	TryPlayMontage_Local(FireMontage, TEXT("FIRE"));
}

void APlayerCharacter::Multicast_PlayHitReactMontage_Implementation()
{
	TryPlayMontage_Local(HitReactMontage, TEXT("HIT"));
}

void APlayerCharacter::Multicast_PlayDeathMontage_Implementation()
{
	TryPlayMontage_Local(DeathMontage, TEXT("DEATH"));
}
