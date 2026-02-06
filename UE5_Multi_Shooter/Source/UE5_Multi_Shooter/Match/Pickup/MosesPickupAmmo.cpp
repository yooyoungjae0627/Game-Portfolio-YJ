#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmo.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h"

#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmoData.h"

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

	// 선택: DataAsset에 WorldMesh가 있으면 적용
	if (PickupData && PickupData->WorldMesh.IsValid())
	{
		Mesh->SetStaticMesh(PickupData->WorldMesh.Get());
	}
	else if (PickupData && !PickupData->WorldMesh.IsNull())
	{
		if (UStaticMesh* Loaded = PickupData->WorldMesh.LoadSynchronous())
		{
			Mesh->SetStaticMesh(Loaded);
		}
	}
}

bool AMosesPickupAmmo::ServerTryPickup(AMosesPlayerState* PickerPS, FText& OutAnnounceText)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!PickerPS || !PickupData)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] AmmoPickup FAIL (Null PS/Data) Actor=%s"), *GetNameSafe(this));
		return false;
	}

	if (!PickupData->AmmoTypeId.IsValid())
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] AmmoPickup FAIL (AmmoTypeId invalid) Actor=%s"), *GetNameSafe(this));
		return false;
	}

	UMosesCombatComponent* Combat = PickerPS->GetCombatComponent();
	if (!Combat)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] AmmoPickup FAIL (NoCombat) PS=%s"), *GetNameSafe(PickerPS));
		return false;
	}

	// ✅ 서버에서만 적용 (SSOT)
	// 현재 너 CombatComponent는 enum 기반(ServerAddAmmoByType)을 쓰고 있으니
	// Tag 기반으로 통일했다면 여기서 ServerAddAmmoByTag로 호출하면 됨.
	//
	// 지금은 "Tag 기반"을 권장하므로, CombatComponent에 아래 함수가 있다고 가정:
	//   void ServerAddAmmoByTag(FGameplayTag AmmoTypeId, int32 ReserveMaxDelta, int32 ReserveFillDelta);
	//
	Combat->ServerAddAmmoByTag(PickupData->AmmoTypeId, PickupData->ReserveMaxDelta, PickupData->ReserveFillDelta);

	OutAnnounceText = FText::FromString(FString::Printf(TEXT("%s 획득"), *PickupData->DisplayName.ToString()));

	UE_LOG(LogMosesPickup, Warning,
		TEXT("[PICKUP][SV] AmmoPickup OK TypeId=%s MaxDelta=%d FillDelta=%d PS=%s Actor=%s"),
		*PickupData->AmmoTypeId.ToString(),
		PickupData->ReserveMaxDelta,
		PickupData->ReserveFillDelta,
		*GetNameSafe(PickerPS),
		*GetNameSafe(this));

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
