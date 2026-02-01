// ============================================================================
// MosesPickupWeapon.cpp (FULL)  [MOD + Billboard]
// ============================================================================

#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeaponData.h"

#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesSlotOwnershipComponent.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Player/MosesInteractionComponent.h"
#include "UE5_Multi_Shooter/UI/Match/MosesPickupPromptWidget.h"

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
	PromptWidgetComponent->SetVisibility(false);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
}

void AMosesPickupWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (InteractSphere)
	{
		InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereBeginOverlap);
		InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereEndOverlap);
	}

	SetPromptVisible_Local(false);
	StopPromptBillboard_Local();

	if (PickupData && PickupData->WorldMesh.IsValid())
	{
		Mesh->SetSkeletalMesh(PickupData->WorldMesh.Get());
	}

	ApplyPromptText_Local();
}

void AMosesPickupWeapon::SetLocalHighlight(bool bEnable)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bEnable);
	}
}

bool AMosesPickupWeapon::ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText)
{
	OutAnnounceText = FText::GetEmpty();

	if (!HasAuthority())
	{
		return false;
	}

	if (!CanPickup_Server(RequesterPS))
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] FAIL Guard Actor=%s Player=%s"),
			*GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	if (bConsumed)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] FAIL AlreadyConsumed Actor=%s"), *GetNameSafe(this));
		return false;
	}

	if (!PickupData)
	{
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL NoPickupData Actor=%s"), *GetNameSafe(this));
		return false;
	}

	UMosesSlotOwnershipComponent* Slots = RequesterPS ? RequesterPS->GetSlotOwnershipComponent() : nullptr;
	if (!Slots)
	{
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL MissingSlots Actor=%s Player=%s"),
			*GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	bConsumed = true;

	Slots->ServerAcquireSlot(PickupData->SlotIndex, PickupData->ItemId);

	const FText NameText = !PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->ItemId.ToString());

	OutAnnounceText = FText::Format(FText::FromString(TEXT("{0} 획득")), NameText);

	UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP][SV] OK Player=%s Slot=%d Item=%s"),
		*GetNameSafe(RequesterPS), PickupData->SlotIndex, *PickupData->ItemId.ToString());

	Destroy();
	return true;
}

void AMosesPickupWeapon::HandleSphereBeginOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	if (Pawn->IsLocallyControlled())
	{
		LocalPromptPawn = Pawn;

		SetLocalHighlight(true);
		ApplyPromptText_Local();
		SetPromptVisible_Local(true);
		StartPromptBillboard_Local();

		if (UMosesInteractionComponent* IC = GetInteractionComponentFromPawn(Pawn))
		{
			IC->SetCurrentInteractTarget_Local(this);
		}

		UE_LOG(LogMosesPickup, Verbose, TEXT("[PICKUP][CL] Prompt+Target Show Actor=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(Pawn));
	}
}

void AMosesPickupWeapon::HandleSphereEndOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	if (Pawn->IsLocallyControlled())
	{
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

		UE_LOG(LogMosesPickup, Verbose, TEXT("[PICKUP][CL] Prompt+Target Hide Actor=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(Pawn));
	}
}

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

	const FText InteractText = !PickupData->InteractText.IsEmpty()
		? PickupData->InteractText
		: FText::FromString(TEXT("E : 줍기"));

	const FText NameText = !PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->ItemId.ToString());

	Prompt->SetPromptTexts(InteractText, NameText);
}

bool AMosesPickupWeapon::CanPickup_Server(const AMosesPlayerState* RequesterPS) const
{
	return (RequesterPS != nullptr && PickupData != nullptr);
}

UMosesInteractionComponent* AMosesPickupWeapon::GetInteractionComponentFromPawn(APawn* Pawn) const
{
	return Pawn ? Pawn->FindComponentByClass<UMosesInteractionComponent>() : nullptr;
}

// ============================================================================
// [MOD] Billboard (Local only)
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

	UE_LOG(LogMosesPickup, VeryVerbose, TEXT("[PICKUP][CL] Billboard START Actor=%s"), *GetNameSafe(this));
}

void AMosesPickupWeapon::StopPromptBillboard_Local()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_PromptBillboard))
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_PromptBillboard);
		UE_LOG(LogMosesPickup, VeryVerbose, TEXT("[PICKUP][CL] Billboard STOP Actor=%s"), *GetNameSafe(this));
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
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
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

	// ✅ UI는 눕지 않게 Yaw만 권장
	LookRot.Pitch = 0.f;
	LookRot.Roll = 0.f;

	PromptWidgetComponent->SetWorldRotation(LookRot);
}
