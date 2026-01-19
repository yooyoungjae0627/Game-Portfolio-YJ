#include "UE5_Multi_Shooter/Pickup/PickupBase.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

APickupBase::APickupBase()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
}

void APickupBase::BeginPlay()
{
	Super::BeginPlay();
}

void APickupBase::ServerTryConsume(APlayerCharacter* RequestingPawn)
{
	// 1) 서버만 확정 (클라에서 호출되어도 무시)
	if (!HasAuthority())
	{
		return;
	}

	// 2) 원자성 잠금: 처리 중이거나 이미 소비됐으면 즉시 실패
	if (bProcessing || bConsumed)
	{
		UE_LOG(LogMosesPickup, Log, TEXT("[Pickup][SV] Reject(Processing/Consumed) Target=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(RequestingPawn));
		return;
	}

	bProcessing = true;

	// 3) 거리/유효성 검사
	if (!CanConsume_Server(RequestingPawn))
	{
		bProcessing = false;

		UE_LOG(LogMosesPickup, Log, TEXT("[Pickup][SV] Reject(Invalid/TooFar) Target=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(RequestingPawn));
		return;
	}

	// 4) 보상 적용 시도 (핵심 변경점)
	// - 자식 클래스(APickup_Ammo 등)가 override한 Server_ApplyPickup()에서
	//   실제 지급/적용을 수행하고 성공/실패를 반환한다.
	const bool bApplied = Server_ApplyPickup(RequestingPawn); // [MODIFIED]

	if (!bApplied)
	{
		// 적용 실패면 소비 확정하면 안 된다.
		bProcessing = false;

		UE_LOG(LogMosesPickup, Log, TEXT("[Pickup][SV] Reject(ApplyFailed) Target=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(RequestingPawn));
		return;
	}

	// 5) 여기부터 성공 확정
	bConsumed = true;

	UE_LOG(LogMosesPickup, Warning, TEXT("[Pickup][SV] SUCCESS Consume Target=%s Pawn=%s"),
		*GetNameSafe(this), *GetNameSafe(RequestingPawn));

	bProcessing = false;

	// 6) 기본 동작: 소비되면 제거
	// - 나중에 Respawn/재생성 시스템이 생기면 여기만 바꾸면 된다.
	Destroy();
}


bool APickupBase::CanConsume_Server(APlayerCharacter* RequestingPawn) const
{
	if (!RequestingPawn)
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(RequestingPawn->GetActorLocation(), GetActorLocation());
	return DistSq <= FMath::Square(ConsumeDistance);
}

void APickupBase::ApplyReward_Server(APlayerCharacter* RequestingPawn)
{
	// 베이스는 "보상 없음" (자식에서 반드시 override 추천)
	UE_LOG(LogMosesPickup, Log, TEXT("[Pickup][SV] ApplyReward(Base) Target=%s Pawn=%s"),
		*GetNameSafe(this), *GetNameSafe(RequestingPawn));
}

bool APickupBase::Server_ApplyPickup(APawn* InstigatorPawn)
{
	// 기본 구현은 "아무것도 안 함"
	// 자식(APickup_Ammo 등)이 override해서 실제 지급을 수행
	return false;
}
