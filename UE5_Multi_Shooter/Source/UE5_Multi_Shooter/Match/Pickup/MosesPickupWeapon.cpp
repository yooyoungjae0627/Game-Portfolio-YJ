// ============================================================================
// UE5_Multi_Shooter/Match/Pickup/MosesPickupWeapon.cpp  (FULL · FINAL)
// ============================================================================

#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupWeapon.h"

#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupWeaponData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Match/Components/MosesSlotOwnershipComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h"

#include "UE5_Multi_Shooter/Match/UI/Match/MosesPickupPromptWidget.h"

#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ============================================================================
// ctor
// ============================================================================

AMosesPickupWeapon::AMosesPickupWeapon()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRenderCustomDepth(false);
	Mesh->bCastDynamicShadow = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(Root);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractSphere->SetGenerateOverlapEvents(true);

	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(Root);
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetTwoSided(true);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	PromptWidgetComponent->SetVisibility(false);
}

// ============================================================================
// BeginPlay
// ============================================================================

void AMosesPickupWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (InteractSphere)
	{
		InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleSphereBeginOverlap);
		InteractSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleSphereEndOverlap);
	}

	SetPromptVisible_Local(false);
	StopPromptBillboard_Local();

	if (PickupData && PickupData->WorldMesh.IsValid())
	{
		Mesh->SetSkeletalMesh(PickupData->WorldMesh.Get());
	}

	ApplyPromptText_Local();
}

// ============================================================================
// Server: Pickup
// ============================================================================

bool AMosesPickupWeapon::ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText)
{
	OutAnnounceText = FText::GetEmpty();

	if (!HasAuthority())
	{
		return false;
	}

	if (!CanPickup_Server(RequesterPS))
	{
		UE_LOG(LogMosesPickup, Warning,
			TEXT("[PICKUP][SV] Weapon FAIL Guard Actor=%s Player=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));
		return false;
	}

	if (bConsumed)
	{
		UE_LOG(LogMosesPickup, Warning,
			TEXT("[PICKUP][SV] Weapon FAIL AlreadyConsumed Actor=%s"),
			*GetNameSafe(this));
		return false;
	}

	if (!PickupData)
	{
		UE_LOG(LogMosesPickup, Error,
			TEXT("[PICKUP][SV] Weapon FAIL NoPickupData Actor=%s"),
			*GetNameSafe(this));
		return false;
	}

	UMosesSlotOwnershipComponent* Slots =
		RequesterPS ? RequesterPS->GetSlotOwnershipComponent() : nullptr;
	if (!Slots)
	{
		UE_LOG(LogMosesPickup, Error,
			TEXT("[PICKUP][SV] Weapon FAIL MissingSlotOwnership PS=%s"),
			*GetNameSafe(RequesterPS));
		return false;
	}

	UMosesCombatComponent* Combat =
		RequesterPS ? RequesterPS->GetCombatComponent() : nullptr;
	if (!Combat)
	{
		UE_LOG(LogMosesPickup, Error,
			TEXT("[PICKUP][SV] Weapon FAIL MissingCombatComponent PS=%s"),
			*GetNameSafe(RequesterPS));
		return false;
	}

	// ---------------------------------------------------------------------
	// 원자성 보장 (OK 1 / FAIL 1)
	// ---------------------------------------------------------------------
	bConsumed = true;

	// 1) 슬롯 소유 기록 (보조 정보)
	Slots->ServerAcquireSlot(PickupData->SlotIndex, PickupData->ItemId);

	// 2) Combat SSOT 갱신 (STEP4 핵심)
	Combat->ServerGrantWeaponToSlot(
		PickupData->SlotIndex,
		PickupData->ItemId,
		/*bInitializeAmmoIfEmpty*/ true);

	const FText NameText =
		!PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->ItemId.ToString());

	OutAnnounceText = FText::Format(
		FText::FromString(TEXT("{0} 획득")),
		NameText);

	UE_LOG(LogMosesPickup, Warning,
		TEXT("[PICKUP][SV][STEP4] Weapon OK Player=%s Slot=%d Item=%s"),
		*GetNameSafe(RequesterPS),
		PickupData->SlotIndex,
		*PickupData->ItemId.ToString());

	Destroy();
	return true;
}

bool AMosesPickupWeapon::CanPickup_Server(const AMosesPlayerState* RequesterPS) const
{
	return (RequesterPS != nullptr && PickupData != nullptr);
}

// ============================================================================
// Local highlight
// ============================================================================

void AMosesPickupWeapon::SetLocalHighlight(bool bEnable)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bEnable);
	}
}

// ============================================================================
// Overlap handlers (Local)
// ============================================================================

void AMosesPickupWeapon::HandleSphereBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	LocalPromptPawn = Pawn;

	SetLocalHighlight(true);
	ApplyPromptText_Local();
	SetPromptVisible_Local(true);
	StartPromptBillboard_Local();

	if (UMosesInteractionComponent* IC = GetInteractionComponentFromPawn(Pawn))
	{
		IC->SetCurrentInteractTarget_Local(this);
	}

	UE_LOG(LogMosesPickup, Verbose,
		TEXT("[PICKUP][CL] Weapon Prompt SHOW Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

void AMosesPickupWeapon::HandleSphereEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (LocalPromptPawn.Get() == Pawn)
	{
		LocalPromptPawn = nullptr;
	}

	SetLocalHighlight(false);
	SetPromptVisible_Local(false);
	StopPromptBillboard_Local();

	if (UMosesInteractionComponent* IC = GetInteractionComponentFromPawn(Pawn))
	{
		IC->ClearCurrentInteractTarget_Local(this);
	}

	UE_LOG(LogMosesPickup, Verbose,
		TEXT("[PICKUP][CL] Weapon Prompt HIDE Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

// ============================================================================
// Prompt UI
// ============================================================================

void AMosesPickupWeapon::SetPromptVisible_Local(bool bVisible)
{
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetVisibility(bVisible);
	}
}

void AMosesPickupWeapon::ApplyPromptText_Local()
{
	if (!PromptWidgetComponent || !PickupData)
	{
		return;
	}

	UUserWidget* Widget = PromptWidgetComponent->GetUserWidgetObject();
	UMosesPickupPromptWidget* Prompt = Cast<UMosesPickupPromptWidget>(Widget);
	if (!Prompt)
	{
		return;
	}

	const FText InteractText =
		!PickupData->InteractText.IsEmpty()
		? PickupData->InteractText
		: FText::FromString(TEXT("E : 줍기"));

	const FText NameText =
		!PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->ItemId.ToString());

	Prompt->SetPromptTexts(InteractText, NameText);
}

UMosesInteractionComponent* AMosesPickupWeapon::GetInteractionComponentFromPawn(APawn* Pawn) const
{
	return Pawn ? Pawn->FindComponentByClass<UMosesInteractionComponent>() : nullptr;
}

// ============================================================================
// Billboard (Local only)
// ============================================================================

void AMosesPickupWeapon::StartPromptBillboard_Local()
{
	if (HasAuthority())
	{
		return;
	}

	if (!PromptWidgetComponent || !PromptWidgetComponent->IsVisible())
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(TimerHandle_PromptBillboard))
	{
		return;
	}

	ApplyBillboardRotation_Local();

	GetWorldTimerManager().SetTimer(
		TimerHandle_PromptBillboard,
		this,
		&ThisClass::TickPromptBillboard_Local,
		PromptBillboardInterval,
		true);
}

void AMosesPickupWeapon::StopPromptBillboard_Local()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_PromptBillboard))
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_PromptBillboard);
	}
}

void AMosesPickupWeapon::TickPromptBillboard_Local()
{
	ApplyBillboardRotation_Local();
}

void AMosesPickupWeapon::ApplyBillboardRotation_Local()
{
	if (!PromptWidgetComponent || !PromptWidgetComponent->IsVisible())
	{
		return;
	}

	APawn* Pawn = LocalPromptPawn.Get();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector MyLoc = PromptWidgetComponent->GetComponentLocation();

	FVector ToCam = CamLoc - MyLoc;
	if (ToCam.IsNearlyZero())
	{
		return;
	}

	FRotator LookRot = ToCam.Rotation();
	LookRot.Pitch = 0.f;
	LookRot.Roll = 0.f;

	PromptWidgetComponent->SetWorldRotation(LookRot);
}
