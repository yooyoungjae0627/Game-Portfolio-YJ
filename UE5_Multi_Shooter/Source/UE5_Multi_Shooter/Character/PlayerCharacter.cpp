#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"
#include "UE5_Multi_Shooter/Pickup/PickupBase.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "UE5_Multi_Shooter/Combat/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

APlayerCharacter::APlayerCharacter()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	bFindCameraComponentWhenViewTarget = true;

	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));
	MosesCameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("MosesCameraComponent"));

	if (MosesCameraComponent)
	{
		MosesCameraComponent->SetupAttachment(GetRootComponent());
		MosesCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		MosesCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		MosesCameraComponent->bAutoActivate = true;
	}

	// [ADD] WeaponMeshComp는 생성만 해두고 BeginPlay에서 Socket에 부착
	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	if (WeaponMeshComp)
	{
		WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMeshComp->SetGenerateOverlapEvents(false);
		WeaponMeshComp->SetCastShadow(true);
		WeaponMeshComp->PrimaryComponentTick.bCanEverTick = false;
		WeaponMeshComp->SetupAttachment(GetMesh()); // 실제 Socket 부착은 BeginPlay에서
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}

	EnsureWeaponMeshComponent();

	BindCombatComponent();
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCombatComponent();

	// 코스메틱 정리(메시만 비움)
	if (WeaponMeshComp)
	{
		WeaponMeshComp->SetStaticMesh(nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (HeroComponent)
	{
		HeroComponent->SetupInputBindings(PlayerInputComponent);
	}
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
}

void APlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (HeroComponent)
	{
		HeroComponent->TryBindCameraModeDelegate_LocalOnly();
	}
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (HeroComponent)
	{
		HeroComponent->TryBindCameraModeDelegate_LocalOnly();
	}

	BindCombatComponent();
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HeroComponent)
	{
		HeroComponent->TryBindCameraModeDelegate_LocalOnly();
	}

	BindCombatComponent();
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindCombatComponent();
}

bool APlayerCharacter::IsSprinting() const
{
	return IsLocallyControlled() ? bLocalPredictedSprinting : bIsSprinting;
}

void APlayerCharacter::Input_Move(const FVector2D& MoveValue)
{
	if (!Controller || MoveValue.IsNearlyZero())
	{
		return;
	}

	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

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
	ApplySprintSpeed_LocalPredict(true);
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
	APickupBase* Target = FindPickupTarget_Local();
	Server_TryPickup(Target);
}

// -------------------------
// Equip 입력
// -------------------------

void APlayerCharacter::Input_EquipSlot1()
{
	if (CachedCombatComponent)
	{
		CachedCombatComponent->RequestEquipSlot(1);
	}
}

void APlayerCharacter::Input_EquipSlot2()
{
	if (CachedCombatComponent)
	{
		CachedCombatComponent->RequestEquipSlot(2);
	}
}

void APlayerCharacter::Input_EquipSlot3()
{
	if (CachedCombatComponent)
	{
		CachedCombatComponent->RequestEquipSlot(3);
	}
}

// -------------------------
// Sprint
// -------------------------

void APlayerCharacter::ApplySprintSpeed_LocalPredict(bool bNewSprinting)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	bLocalPredictedSprinting = bNewSprinting;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bLocalPredictedSprinting ? SprintSpeed : WalkSpeed;
	}
}

void APlayerCharacter::ApplySprintSpeed_FromAuth(const TCHAR* From)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	if (IsLocallyControlled())
	{
		bLocalPredictedSprinting = bIsSprinting;
	}
}

void APlayerCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	ApplySprintSpeed_FromAuth(TEXT("Server_SetSprinting"));
}

void APlayerCharacter::OnRep_IsSprinting()
{
	ApplySprintSpeed_FromAuth(TEXT("OnRep_IsSprinting"));
}

// -------------------------
// Pickup
// -------------------------

APickupBase* APlayerCharacter::FindPickupTarget_Local() const
{
	if (!IsLocallyControlled())
	{
		return nullptr;
	}

	const UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(this);
	const FVector Start = CamComp ? CamComp->GetComponentLocation() : GetActorLocation();

	const FRotator Rot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	const FVector End = Start + Rot.Vector() * PickupTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MosesPickupTrace), false, this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return nullptr;
	}

	return Cast<APickupBase>(Hit.GetActor());
}

void APlayerCharacter::Server_TryPickup_Implementation(APickupBase* Target)
{
	if (!Target)
	{
		return;
	}

	Target->ServerTryConsume(this);
}

// -------------------------
// Equip Cosmetic
// -------------------------

void APlayerCharacter::BindCombatComponent()
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	UMosesCombatComponent* CC = PS->GetCombatComponent();
	if (!CC || CC == CachedCombatComponent)
	{
		return;
	}

	UnbindCombatComponent();

	CachedCombatComponent = CC;
	CachedCombatComponent->OnEquippedChanged.AddUObject(this, &ThisClass::HandleEquippedChanged);

	const FGameplayTag EquippedId = CachedCombatComponent->GetEquippedWeaponId();
	if (EquippedId.IsValid())
	{
		HandleEquippedChanged(CachedCombatComponent->GetCurrentSlot(), EquippedId);
	}
}

void APlayerCharacter::UnbindCombatComponent()
{
	if (CachedCombatComponent)
	{
		CachedCombatComponent->OnEquippedChanged.RemoveAll(this);
		CachedCombatComponent = nullptr;
	}
}

void APlayerCharacter::HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId)
{
	RefreshWeaponCosmetic(WeaponId);
}

void APlayerCharacter::EnsureWeaponMeshComponent()
{
	if (!WeaponMeshComp)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// Socket 존재 여부는 여기서 한 번만 체크(없으면 이후 Attach 실패 로그가 난다)
	if (!MeshComp->DoesSocketExist(CharacterWeaponSocketName))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][CL] Missing Character Socket=%s Char=%s"),
			*CharacterWeaponSocketName.ToString(), *GetNameSafe(this));
		return;
	}

	WeaponMeshComp->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CharacterWeaponSocketName);
}

void APlayerCharacter::RefreshWeaponCosmetic(FGameplayTag WeaponId)
{
	if (!WeaponMeshComp)
	{
		return;
	}

	if (!WeaponId.IsValid())
	{
		WeaponMeshComp->SetStaticMesh(nullptr);

		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][CL] OnRep Equip FAIL WeaponId=INVALID Char=%s"),
			*GetNameSafe(this));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][CL] OnRep Equip FAIL NoGameInstance Char=%s Weapon=%s"),
			*GetNameSafe(this), *WeaponId.ToString());
		return;
	}

	UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][CL] OnRep Equip FAIL NoRegistry Char=%s Weapon=%s"),
			*GetNameSafe(this), *WeaponId.ToString());
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][CL] OnRep Equip FAIL ResolveData Weapon=%s Char=%s"),
			*WeaponId.ToString(), *GetNameSafe(this));
		return;
	}

	UStaticMesh* StaticMesh = Data->WeaponStaticMesh.LoadSynchronous();
	if (!StaticMesh)
	{
		WeaponMeshComp->SetStaticMesh(nullptr);

		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][CL] OnRep Equip FAIL WeaponStaticMesh NULL Weapon=%s Data=%s"),
			*WeaponId.ToString(), *GetNameSafe(Data));
		return;
	}

	WeaponMeshComp->SetStaticMesh(StaticMesh);

	// Socket 재부착 보장(SeamlessTravel/Restart에서 컴포넌트가 풀릴 수 있음)
	EnsureWeaponMeshComponent();

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][CL] OnRep Equip Weapon=%s Attach=OK Char=%s Socket=%s Mesh=%s"),
		*WeaponId.ToString(),
		*GetNameSafe(this),
		*CharacterWeaponSocketName.ToString(),
		*GetNameSafe(StaticMesh));
}
