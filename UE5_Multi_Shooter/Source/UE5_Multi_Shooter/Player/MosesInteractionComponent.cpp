// ============================================================================
// MosesInteractionComponent.cpp (FULL)  [MOD]
// ============================================================================

#include "UE5_Multi_Shooter/Player/MosesInteractionComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

UMosesInteractionComponent::UMosesInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// [MOD] Overlap 기반이므로 타이머/트레이스 없음.
	// 로컬 타겟은 Pickup/Flag의 Overlap에서 Set/Clear 된다.
	CachedTarget = FMosesInteractTarget();
}

void UMosesInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// [MOD] 타이머 없음. 정리만 수행.
	CachedTarget = FMosesInteractTarget();
	PressedTarget = nullptr;
	bHoldingInteract = false;

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// [MOD] Target Set/Clear (Local only)
// ============================================================================

void UMosesInteractionComponent::SetCurrentInteractTarget_Local(AActor* NewTarget)
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (!IsValid(NewTarget))
	{
		return;
	}

	const EMosesInteractTargetType NewType = ResolveTargetType(NewTarget);
	if (NewType == EMosesInteractTargetType::None)
	{
		return;
	}

	// 동일 타겟이면 스킵
	if (CachedTarget.TargetActor.Get() == NewTarget && CachedTarget.Type == NewType)
	{
		return;
	}

	CachedTarget.Type = NewType;
	CachedTarget.TargetActor = NewTarget;

	UE_LOG(LogMosesCombat, Verbose, TEXT("[INTERACT][CL] Target SET Type=%d Target=%s Owner=%s"),
		static_cast<int32>(CachedTarget.Type),
		*GetNameSafe(CachedTarget.TargetActor.Get()),
		*GetNameSafe(GetOwner()));
}

void UMosesInteractionComponent::ClearCurrentInteractTarget_Local(AActor* ExpectedTarget)
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (!ExpectedTarget)
	{
		return;
	}

	// 현재 타겟과 같을 때만 해제
	if (CachedTarget.TargetActor.Get() != ExpectedTarget)
	{
		return;
	}

	UE_LOG(LogMosesCombat, Verbose, TEXT("[INTERACT][CL] Target CLEAR Target=%s Owner=%s"),
		*GetNameSafe(ExpectedTarget),
		*GetNameSafe(GetOwner()));

	CachedTarget = FMosesInteractTarget();
}

// ============================================================================
// Input Endpoints
// ============================================================================

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

// ============================================================================
// Server RPC
// ============================================================================

void UMosesInteractionComponent::ServerInteractPressed_Implementation(AActor* TargetActor)
{
	HandleInteractPressed_Server(TargetActor);
}

void UMosesInteractionComponent::ServerInteractReleased_Implementation(AActor* TargetActor)
{
	HandleInteractReleased_Server(TargetActor);
}

// ============================================================================
// Server Handlers
// ============================================================================

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

	// ---------------------------------------------------------------------
	// FlagSpot
	// ---------------------------------------------------------------------
	if (AMosesFlagSpot* Flag = Cast<AMosesFlagSpot>(TargetActor))
	{
		const bool bOK = Flag->ServerTryStartCapture(PS);

		UE_LOG(LogMosesFlag, Log, TEXT("[FLAG][SV] CaptureStartReq Player=%s Spot=%s OK=%d"),
			*GetNameSafe(PS), *GetNameSafe(Flag), bOK ? 1 : 0);

		return;
	}

	// ---------------------------------------------------------------------
	// PickupWeapon
	// ---------------------------------------------------------------------
	if (AMosesPickupWeapon* Pickup = Cast<AMosesPickupWeapon>(TargetActor))
	{
		const APawn* Pawn = GetOwningPawn();
		const float Dist = Pawn ? FVector::Distance(Pawn->GetActorLocation(), Pickup->GetActorLocation()) : -1.0f;

		UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP][SV] Try Player=%s Target=%s Dist=%.1f"),
			*GetNameSafe(PS),
			*GetNameSafe(Pickup),
			Dist);

		FText AnnounceText;
		const bool bOK = Pickup->ServerTryPickup(PS, AnnounceText);

		UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP][SV] Result Player=%s Actor=%s OK=%d"),
			*GetNameSafe(PS), *GetNameSafe(Pickup), bOK ? 1 : 0);

		// 성공 시 중앙 Announcement (RepNotify -> Delegate -> HUD)
		if (bOK)
		{
			AMosesMatchGameState* GS = GetMatchGameState();
			if (GS)
			{
				GS->ServerStartAnnouncementText(AnnounceText, 4);

				UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] Set Text=\"%s\" Dur=4"),
					*AnnounceText.ToString());
			}
			else
			{
				UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] FAIL NoMatchGameState (cannot broadcast)"));
			}
		}

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

// ============================================================================
// Guards / Helpers
// ============================================================================

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

AMosesMatchGameState* UMosesInteractionComponent::GetMatchGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<AMosesMatchGameState>() : nullptr;
}

EMosesInteractTargetType UMosesInteractionComponent::ResolveTargetType(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return EMosesInteractTargetType::None;
	}

	if (TargetActor->IsA<AMosesFlagSpot>())
	{
		return EMosesInteractTargetType::FlagSpot;
	}

	if (TargetActor->IsA<AMosesPickupWeapon>())
	{
		return EMosesInteractTargetType::Pickup;
	}

	return EMosesInteractTargetType::None;
}
