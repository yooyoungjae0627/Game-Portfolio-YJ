// ============================================================================
// MosesPickupWeapon.cpp (FULL)  [MOD]
// ============================================================================

#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeaponData.h"

#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"

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

	// Prompt Widget (World UI)
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

	// Prompt widget는 기본 Hidden
	SetPromptVisible_Local(false);

	// 월드 스켈레탈 메시 적용(선택)
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

	UMosesSlotOwnershipComponent* Slots = RequesterPS ? RequesterPS->GetSlotOwnershipComponent() : nullptr; // [MOD]
	if (!Slots)
	{
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL MissingSlots Actor=%s Player=%s"),
			*GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	// Atomic consume
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

	// 로컬 UX + [MOD] 타겟 세팅은 로컬 Pawn만
	if (Pawn->IsLocallyControlled())
	{
		LocalPromptPawn = Pawn;

		SetLocalHighlight(true);
		ApplyPromptText_Local();
		SetPromptVisible_Local(true);

		// [MOD] Overlap 기반 타겟 등록
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

		// [MOD] Overlap 기반 타겟 해제
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
	if (!RequesterPS || !PickupData)
	{
		return false;
	}
	return true;
}

UMosesInteractionComponent* AMosesPickupWeapon::GetInteractionComponentFromPawn(APawn* Pawn) const
{
	return Pawn ? Pawn->FindComponentByClass<UMosesInteractionComponent>() : nullptr;
}
