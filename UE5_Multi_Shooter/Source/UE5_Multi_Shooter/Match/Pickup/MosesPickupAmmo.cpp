// ============================================================================
// UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmo.cpp  (FULL - UPDATED)
// ============================================================================

#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmo.h"

#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmoData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h"

#include "UE5_Multi_Shooter/Match/UI/Match/MosesPickupPromptWidget.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMosesPickupAmmo::AMosesPickupAmmo()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
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

void AMosesPickupAmmo::BeginPlay()
{
	Super::BeginPlay();

	if (InteractSphere)
	{
		InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleSphereBeginOverlap);
		InteractSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleSphereEndOverlap);
	}

	SetPromptVisible_Local(false);
	StopPromptBillboard_Local();

	// WorldMesh 적용(선택)
	if (PickupData)
	{
		if (PickupData->WorldMesh.IsValid())
		{
			Mesh->SetStaticMesh(PickupData->WorldMesh.Get());
		}
		else if (!PickupData->WorldMesh.IsNull())
		{
			if (UStaticMesh* Loaded = PickupData->WorldMesh.LoadSynchronous())
			{
				Mesh->SetStaticMesh(Loaded);
			}
		}
	}

	ApplyPromptText_Local();
}

void AMosesPickupAmmo::SetLocalHighlight(bool bEnable)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bEnable);
	}
}

bool AMosesPickupAmmo::ServerTryPickup(AMosesPlayerState* PickerPS, FText& OutAnnounceText)
{
	OutAnnounceText = FText::GetEmpty();

	if (!HasAuthority())
	{
		return false;
	}

	if (bConsumed)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] Ammo FAIL AlreadyConsumed Actor=%s"), *GetNameSafe(this));
		return false;
	}

	if (!PickerPS || !PickupData)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] Ammo FAIL (Null PS/Data) Actor=%s"), *GetNameSafe(this));
		return false;
	}

	if (!PickupData->AmmoTypeId.IsValid())
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] Ammo FAIL (AmmoTypeId invalid) Actor=%s"), *GetNameSafe(this));
		return false;
	}

	UMosesCombatComponent* Combat = PickerPS->GetCombatComponent();
	if (!Combat)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] Ammo FAIL (NoCombat) PS=%s"), *GetNameSafe(PickerPS));
		return false;
	}

	// -------------------------------------------------------------------------
	// 원자성 판정: OK 1 / FAIL 1
	// -------------------------------------------------------------------------
	bConsumed = true;

	// ✅ 서버에서만 SSOT 갱신
	// - 네 CombatComponent에 ServerAddAmmoByTag가 이미 추가된 상태 기준
	Combat->ServerAddAmmoByTag(PickupData->AmmoTypeId, PickupData->ReserveMaxDelta, PickupData->ReserveFillDelta);

	const FText NameText = !PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->AmmoTypeId.ToString());

	OutAnnounceText = FText::Format(FText::FromString(TEXT("{0} 획득")), NameText);

	UE_LOG(LogMosesPickup, Warning,
		TEXT("[PICKUP][SV] Ammo OK TypeId=%s MaxDelta=%d FillDelta=%d PS=%s Actor=%s"),
		*PickupData->AmmoTypeId.ToString(),
		PickupData->ReserveMaxDelta,
		PickupData->ReserveFillDelta,
		*GetNameSafe(PickerPS),
		*GetNameSafe(this));

	Destroy();
	return true;
}

void AMosesPickupAmmo::HandleSphereBeginOverlap(
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

		UE_LOG(LogMosesPickup, Verbose, TEXT("[PICKUP][CL] Ammo Prompt+Target Show Actor=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(Pawn));
	}
}

void AMosesPickupAmmo::HandleSphereEndOverlap(
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

		UE_LOG(LogMosesPickup, Verbose, TEXT("[PICKUP][CL] Ammo Prompt+Target Hide Actor=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(Pawn));
	}
}

void AMosesPickupAmmo::SetPromptVisible_Local(bool bVisible)
{
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetVisibility(bVisible);
	}
}

void AMosesPickupAmmo::ApplyPromptText_Local()
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
		: FText::FromString(PickupData->AmmoTypeId.ToString());

	Prompt->SetPromptTexts(InteractText, NameText);
}

UMosesInteractionComponent* AMosesPickupAmmo::GetInteractionComponentFromPawn(APawn* Pawn) const
{
	return Pawn ? Pawn->FindComponentByClass<UMosesInteractionComponent>() : nullptr;
}

// ============================================================================
// Billboard (Local only) - Tick 금지, Timer 기반
// ============================================================================

void AMosesPickupAmmo::StartPromptBillboard_Local()
{
	// 로컬에서만
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

	UE_LOG(LogMosesPickup, VeryVerbose, TEXT("[PICKUP][CL] Ammo Billboard START Actor=%s"), *GetNameSafe(this));
}

void AMosesPickupAmmo::StopPromptBillboard_Local()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_PromptBillboard))
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_PromptBillboard);
		UE_LOG(LogMosesPickup, VeryVerbose, TEXT("[PICKUP][CL] Ammo Billboard STOP Actor=%s"), *GetNameSafe(this));
	}
}

void AMosesPickupAmmo::TickPromptBillboard_Local()
{
	ApplyBillboardRotation_Local();
}

void AMosesPickupAmmo::ApplyBillboardRotation_Local()
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
	LookRot.Pitch = 0.f;
	LookRot.Roll = 0.f;

	PromptWidgetComponent->SetWorldRotation(LookRot);
}
