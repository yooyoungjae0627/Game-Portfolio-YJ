// ============================================================================
// UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.cpp  (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Match/Flag/MosesFlagSpot.h"
#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupAmmo.h"

#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UMosesInteractionComponent::UMosesInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Overlap 기반이므로 타이머/트레이스 없음.
	CachedTarget = FMosesInteractTarget();
}

void UMosesInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 남아있는 홀드가 있으면 로컬/서버 모두 정리
	ForceReleaseInteract_Local(nullptr, TEXT("EndPlay"));

	CachedTarget = FMosesInteractTarget();
	PressedTarget = nullptr;
	bHoldingInteract = false;

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// Target Set/Clear (Local only)
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

	// 타겟을 잃는 순간 홀드 상태가 남아있으면 다음 Press가 막힌다 → 강제 Release
	ForceReleaseInteract_Local(ExpectedTarget, TEXT("TargetLost"));

	CachedTarget = FMosesInteractTarget();
}

// ============================================================================
// Input Endpoints (E Press/Release)
// ============================================================================

void UMosesInteractionComponent::RequestInteractPressed()
{
	if (bHoldingInteract)
	{
		return;
	}

	// 타겟이 없으면 입력 무시
	if (!CachedTarget.TargetActor.IsValid() || CachedTarget.Type == EMosesInteractTargetType::None)
	{
		UE_LOG(LogMosesCombat, VeryVerbose, TEXT("[INTERACT][CL] Press IGNORE (NoTarget) Owner=%s"),
			*GetNameSafe(GetOwner()));
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

	// ReleaseTarget가 null이면 서버 RPC 날리지 않음
	if (!ReleaseTarget)
	{
		return;
	}

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
		// 서버에서 "실제 Zone 안"을 1회 더 확인 (stale target 방지)
		const APawn* Pawn = GetOwningPawn();
		bool bInZone = false;

		if (Pawn)
		{
			if (USphereComponent* Zone = Flag->GetCaptureZone())
			{
				bInZone = Zone->IsOverlappingActor(Pawn);
			}
		}

		if (!bInZone)
		{
			UE_LOG(LogMosesCombat, Warning, TEXT("[INTERACT][SV] Press REJECT NotInZone Player=%s Target=%s"),
				*GetNameSafe(PS), *GetNameSafe(TargetActor));
			return;
		}

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

	// ✅ PickupAmmo
	if (AMosesPickupAmmo* Ammo = Cast<AMosesPickupAmmo>(TargetActor))
	{
		FText AnnounceText;
		const bool bOK = Ammo->ServerTryPickup(PS, AnnounceText);

		if (bOK)
		{
			if (AMosesMatchGameState* GS = GetMatchGameState())
			{
				GS->ServerStartAnnouncementText(AnnounceText, 4);
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

	// 토글 캡처: Release는 취소가 아니다.
	UE_LOG(LogMosesCombat, Verbose, TEXT("[INTERACT][SV] Release IGNORE (ToggleCapture) Player=%s Target=%s"),
		*GetNameSafe(PS),
		*GetNameSafe(TargetActor));
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

	FVector TargetLoc = TargetActor->GetActorLocation();

	// FlagSpot은 CaptureZone 위치 기준으로 거리 판정
	if (const AMosesFlagSpot* FlagSpot = Cast<AMosesFlagSpot>(TargetActor))
	{
		if (const USphereComponent* Zone = FlagSpot->GetCaptureZone())
		{
			TargetLoc = Zone->GetComponentLocation();
		}
	}

	const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), TargetLoc);
	const bool bOK = (DistSq <= FMath::Square(MaxUseDistance));

	UE_LOG(LogMosesCombat, Warning, TEXT("[INTERACT][SV][DIST] OK=%d Dist=%.1f Max=%.1f PlayerPawn=%s Target=%s"),
		bOK ? 1 : 0,
		FMath::Sqrt(DistSq),
		MaxUseDistance,
		*GetNameSafe(Pawn),
		*GetNameSafe(TargetActor));

	return bOK;
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
		return EMosesInteractTargetType::PickupWeapon;
	}

	if (TargetActor->IsA<AMosesPickupAmmo>())
	{
		return EMosesInteractTargetType::PickupAmmo;
	}

	return EMosesInteractTargetType::None;
}

void UMosesInteractionComponent::ForceReleaseInteract_Local(AActor* ExpectedTarget, const TCHAR* ReasonTag)
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	// 홀드 중이 아니면 할 게 없음
	if (!bHoldingInteract)
	{
		return;
	}

	// ExpectedTarget이 있으면 "그 타겟을 누르고 있던 상황"일 때만 풀어준다.
	// (ExpectedTarget=nullptr이면 무조건 풀어도 됨)
	if (ExpectedTarget && PressedTarget.Get() != ExpectedTarget)
	{
		return;
	}

	// 로컬 상태 먼저 해제
	bHoldingInteract = false;

	AActor* ReleaseTarget = PressedTarget.Get();
	PressedTarget = nullptr;

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[INTERACT][CL][FORCE_RELEASE] Reason=%s Target=%s Owner=%s"),
		ReasonTag ? ReasonTag : TEXT("None"),
		*GetNameSafe(ReleaseTarget),
		*GetNameSafe(GetOwner()));

	// 서버에도 Release 전달
	if (ReleaseTarget)
	{
		ServerInteractReleased(ReleaseTarget);
	}
}
