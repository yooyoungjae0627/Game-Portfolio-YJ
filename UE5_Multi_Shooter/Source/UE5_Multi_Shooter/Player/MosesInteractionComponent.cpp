// ============================================================================
// MosesInteractionComponent.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Player/MosesInteractionComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

UMosesInteractionComponent::UMosesInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어만 대상 갱신
	if (APawn* Pawn = GetOwningPawn())
	{
		if (Pawn->IsLocallyControlled())
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					TimerHandle_UpdateTarget,
					this,
					&UMosesInteractionComponent::UpdateInteractTarget_Local,
					UpdateInterval,
					true);
			}
		}
	}
}

void UMosesInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_UpdateTarget);
	}

	Super::EndPlay(EndPlayReason);
}

void UMosesInteractionComponent::RequestInteractPressed()
{
	if (bHoldingInteract)
	{
		return;
	}

	bHoldingInteract = true;
	PressedTarget = CachedTarget.TargetActor;

	UE_LOG(LogMosesCombat, Verbose, TEXT("[INTERACT][CL] Press Target=%s Type=%d"),
		*GetNameSafe(PressedTarget.Get()), static_cast<int32>(CachedTarget.Type));

	ServerInteractPressed(PressedTarget.Get());
}

void UMosesInteractionComponent::RequestInteractReleased()
{
	if (!bHoldingInteract)
	{
		return;
	}

	bHoldingInteract = false;

	AActor* ReleaseTarget = PressedTarget.Get();
	PressedTarget = nullptr;

	UE_LOG(LogMosesCombat, Verbose, TEXT("[INTERACT][CL] Release Target=%s"), *GetNameSafe(ReleaseTarget));

	ServerInteractReleased(ReleaseTarget);
}

void UMosesInteractionComponent::UpdateInteractTarget_Local()
{
	CachedTarget = FindBestTarget_Local();
}

FMosesInteractTarget UMosesInteractionComponent::FindBestTarget_Local() const
{
	FMosesInteractTarget Out;

	APlayerController* PC = GetOwningPC();
	APawn* Pawn = GetOwningPawn();
	if (!PC || !Pawn || !GetWorld())
	{
		return Out;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector Start = ViewLoc;
	const FVector End = Start + (ViewRot.Vector() * TraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MosesInteractTrace), false);
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit || !Hit.GetActor())
	{
		return Out;
	}

	AActor* HitActor = Hit.GetActor();

	if (HitActor->IsA<AMosesFlagSpot>())
	{
		Out.Type = EMosesInteractTargetType::FlagSpot;
		Out.TargetActor = HitActor;
	}
	else if (HitActor->IsA<AMosesPickupWeapon>())
	{
		Out.Type = EMosesInteractTargetType::Pickup;
		Out.TargetActor = HitActor;
	}

	return Out;
}

void UMosesInteractionComponent::ServerInteractPressed_Implementation(AActor* TargetActor)
{
	HandleInteractPressed_Server(TargetActor);
}

void UMosesInteractionComponent::ServerInteractReleased_Implementation(AActor* TargetActor)
{
	HandleInteractReleased_Server(TargetActor);
}

void UMosesInteractionComponent::HandleInteractPressed_Server(AActor* TargetActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerState();
	if (!PS || !TargetActor)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[INTERACT][SV] Press REJECT Null PS/Target"));
		return;
	}

	if (!IsWithinUseDistance_Server(TargetActor))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[INTERACT][SV] Press REJECT Distance Player=%s Target=%s"),
			*GetNameSafe(PS), *GetNameSafe(TargetActor));
		return;
	}

	if (AMosesFlagSpot* Flag = Cast<AMosesFlagSpot>(TargetActor))
	{
		const bool bOK = Flag->ServerTryStartCapture(PS);
		UE_LOG(LogMosesFlag, Log, TEXT("[FLAG][SV] CaptureStartReq Player=%s Spot=%s OK=%d"),
			*GetNameSafe(PS), *GetNameSafe(Flag), bOK ? 1 : 0);
		return;
	}

	if (AMosesPickupWeapon* Pickup = Cast<AMosesPickupWeapon>(TargetActor))
	{
		const bool bOK = Pickup->ServerTryPickup(PS);
		UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP][SV] PickupReq Player=%s Actor=%s OK=%d"),
			*GetNameSafe(PS), *GetNameSafe(Pickup), bOK ? 1 : 0);
		return;
	}

	UE_LOG(LogMosesCombat, Warning, TEXT("[INTERACT][SV] Press IGNORE Unknown Target=%s"), *GetNameSafe(TargetActor));
}

void UMosesInteractionComponent::HandleInteractReleased_Server(AActor* TargetActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerState();
	if (!PS || !TargetActor)
	{
		return;
	}

	// Release는 캡처 취소 의미만 가진다.
	if (AMosesFlagSpot* Flag = Cast<AMosesFlagSpot>(TargetActor))
	{
		Flag->ServerCancelCapture(PS, EMosesCaptureCancelReason::Released);
		UE_LOG(LogMosesFlag, Log, TEXT("[FLAG][SV] CaptureCancel(Released) Player=%s Spot=%s"),
			*GetNameSafe(PS), *GetNameSafe(Flag));
	}
}

bool UMosesInteractionComponent::IsWithinUseDistance_Server(const AActor* TargetActor) const
{
	const APawn* Pawn = GetOwningPawn();
	if (!Pawn || !TargetActor)
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), TargetActor->GetActorLocation());
	return DistSq <= FMath::Square(MaxUseDistance);
}

APawn* UMosesInteractionComponent::GetOwningPawn() const
{
	return Cast<APawn>(GetOwner());
}

APlayerController* UMosesInteractionComponent::GetOwningPC() const
{
	APawn* Pawn = GetOwningPawn();
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

AMosesPlayerState* UMosesInteractionComponent::GetMosesPlayerState() const
{
	APawn* Pawn = GetOwningPawn();
	return Pawn ? Pawn->GetPlayerState<AMosesPlayerState>() : nullptr;
}
