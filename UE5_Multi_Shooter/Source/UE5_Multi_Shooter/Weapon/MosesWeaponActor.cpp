#include "MosesWeaponActor.h"

#include "Components/SkeletalMeshComponent.h"

AMosesWeaponActor::AMosesWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->bCastDynamicShadow = true;
	WeaponMesh->CastShadow = true;

	bReplicates = false; // 코스메틱은 OnRep로 각 클라에서 로컬 스폰
}

void AMosesWeaponActor::BeginPlay()
{
	Super::BeginPlay();
}
