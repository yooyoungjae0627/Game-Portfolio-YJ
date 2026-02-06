#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmo.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AMosesPickupAmmo::AMosesPickupAmmo()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	RootComponent = Sphere;

	Sphere->InitSphereRadius(80.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnSphereBeginOverlap);
	Sphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OnSphereEndOverlap);
}

void AMosesPickupAmmo::BeginPlay()
{
	Super::BeginPlay();
}

bool AMosesPickupAmmo::ServerTryPickup(AMosesPlayerState* PickerPS, FText& OutAnnounceText)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!PickerPS)
	{
		return false;
	}

	UMosesCombatComponent* Combat = PickerPS->GetCombatComponent();
	if (!Combat)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] AmmoPickup FAIL (NoCombat) PS=%s"), *GetNameSafe(PickerPS));
		return false;
	}

	Combat->ServerAddAmmoByType(AmmoType, ReserveMaxDelta, ReserveFillDelta);

	OutAnnounceText = FText::FromString(FString::Printf(TEXT("%s 획득 (+%d Max, +%d Fill)"),
		*PickupName.ToString(), ReserveMaxDelta, ReserveFillDelta));

	UE_LOG(LogMosesPickup, Warning,
		TEXT("[PICKUP][SV] AmmoPickup OK Type=%d MaxDelta=%d FillDelta=%d PS=%s Actor=%s"),
		(int32)AmmoType, ReserveMaxDelta, ReserveFillDelta, *GetNameSafe(PickerPS), *GetNameSafe(this));

	Destroy();
	return true;
}

void AMosesPickupAmmo::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (UMosesInteractionComponent* Interact = Pawn->FindComponentByClass<UMosesInteractionComponent>())
	{
		Interact->SetCurrentInteractTarget_Local(this);
	}
}

void AMosesPickupAmmo::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (UMosesInteractionComponent* Interact = Pawn->FindComponentByClass<UMosesInteractionComponent>())
	{
		Interact->ClearCurrentInteractTarget_Local(this);
	}
}
