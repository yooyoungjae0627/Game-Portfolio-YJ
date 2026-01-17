#include "UE5_Multi_Shooter/Pickup/PickupBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

APickupBase::APickupBase()
{
	bReplicates = true;
	SetReplicateMovement(true);

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SetRootComponent(SphereCollision);
	SphereCollision->InitSphereRadius(80.f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereCollision->SetCollisionObjectType(ECC_WorldDynamic);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SphereCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APickupBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APickupBase, bConsumed);
}

bool APickupBase::Server_TryConsume(APawn* InstigatorPawn)
{
	check(HasAuthority());

	// ✅ 원자성 핵심 1: 이미 먹힘
	if (bConsumed)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=Consumed Pickup=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(InstigatorPawn));
		return false;
	}

	// ✅ 원자성 핵심 2: 재진입 가드 (RPC 중복/재전송/연타 방어)
	if (bProcessing)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=Processing Pickup=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(InstigatorPawn));
		return false;
	}

	bProcessing = true;

	FString FailReason;
	if (!Server_ValidateConsume(InstigatorPawn, FailReason))
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP/SEC] FAIL Reason=%s Pickup=%s Pawn=%s"),
			*FailReason, *GetNameSafe(this), *GetNameSafe(InstigatorPawn));

		bProcessing = false;
		return false;
	}

	// ✅ 보상 적용(파생 클래스가 구현)
	if (!Server_ApplyPickup(InstigatorPawn))
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=ApplyFailed Pickup=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(InstigatorPawn));

		bProcessing = false;
		return false;
	}

	// ✅ 소비 확정
	bConsumed = true;
	OnRep_Consumed(); // 서버도 즉시 처리

	UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP] OK Pickup=%s Pawn=%s"),
		*GetNameSafe(this), *GetNameSafe(InstigatorPawn));

	// ✅ Destroy는 서버만
	Destroy();

	return true;
}

bool APickupBase::Server_ApplyPickup(APawn* InstigatorPawn)
{
	// Base는 아무 보상도 안 줌 (파생에서 구현)
	return IsValid(InstigatorPawn);
}

bool APickupBase::Server_ValidateConsume(APawn* InstigatorPawn, FString& OutFailReason) const
{
	if (!HasAuthority())
	{
		OutFailReason = TEXT("NoAuthority");
		return false;
	}

	if (!IsValid(InstigatorPawn))
	{
		OutFailReason = TEXT("InvalidPawn");
		return false;
	}

	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		OutFailReason = TEXT("InvalidPickup");
		return false;
	}

	const float Dist = GetDistanceToPawn(InstigatorPawn);
	if (Dist > InteractionRange)
	{
		OutFailReason = FString::Printf(TEXT("TooFar(%.1f>%.1f)"), Dist, InteractionRange);
		return false;
	}

	return true;
}

float APickupBase::GetDistanceToPawn(APawn* InstigatorPawn) const
{
	if (!IsValid(InstigatorPawn))
	{
		return BIG_NUMBER;
	}
	return FVector::Dist(GetActorLocation(), InstigatorPawn->GetActorLocation());
}

void APickupBase::OnRep_Consumed()
{
	// ✅ 클라에서 사라지는 연출/이펙트 등을 넣고 싶으면 여기서 처리
	// Day2 최소는 비워둔다.
}
