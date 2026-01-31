// ============================================================================
// MosesPickupWeapon.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeaponData.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesSlotOwnershipComponent.h"
#include "UE5_MULTI_SHOOTER/Player/MosesPlayerState.h"

AMosesPickupWeapon::AMosesPickupWeapon()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRenderCustomDepth(false);

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(Root);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractSphere->SetGenerateOverlapEvents(true);
}

void AMosesPickupWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (InteractSphere)
	{
		InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereBeginOverlap);
		InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereEndOverlap);
	}

	// 월드 메쉬 적용(선택)
	if (PickupData && PickupData->WorldMesh.IsValid())
	{
		Mesh->SetStaticMesh(PickupData->WorldMesh.Get());
	}
}

void AMosesPickupWeapon::SetLocalHighlight(bool bEnable)
{
	// 로컬 코스메틱: CustomDepth 토글
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bEnable);
	}
}

bool AMosesPickupWeapon::ServerTryPickup(AMosesPlayerState* RequesterPS)
{
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

	// 원자성: 이미 소비됨
	if (bConsumed)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] FAIL AlreadyConsumed Actor=%s"), *GetNameSafe(this));
		return false;
	}

	bConsumed = true;

	UMosesSlotOwnershipComponent* Slots = RequesterPS ? RequesterPS->FindComponentByClass<UMosesSlotOwnershipComponent>() : nullptr;
	if (!Slots || !PickupData)
	{
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL MissingSlotsOrData Actor=%s Player=%s"), *GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	Slots->ServerAcquireSlot(PickupData->SlotIndex, PickupData->ItemId);

	UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP][SV] OK Player=%s Slot=%d Item=%s"),
		*GetNameSafe(RequesterPS), PickupData->SlotIndex, *PickupData->ItemId.ToString());

	Destroy(); // 서버 Destroy -> 클라에도 제거
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
	// 로컬 하이라이트는 클라에서만(편의)
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	if (Pawn->IsLocallyControlled())
	{
		SetLocalHighlight(true);
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
		SetLocalHighlight(false);
	}
}

bool AMosesPickupWeapon::CanPickup_Server(const AMosesPlayerState* RequesterPS) const
{
	if (!RequesterPS || !PickupData)
	{
		return false;
	}

	// PhaseReject / DeadReject를 여기서 추가 가능
	return true;
}
