// ============================================================================
// UE5_Multi_Shooter/Match/Components/MosesInteractionComponent.h  (CLEANED)
// ----------------------------------------------------------------------------
// 역할
// - Overlap 기반 상호작용 타겟 시스템 (Trace/Timer/Tick 금지)
// - PickupWeapon / PickupAmmo / FlagSpot이 Overlap Begin/End에서 로컬 타겟 Set/Clear
// - E Press/Release 입력을 서버 RPC로 전달
// - 서버는 거리/권한 Guard 후 FlagSpot(Capture) 또는 Pickup(Pickup) 처리
//
// 설계 원칙
// - Server Authority 100%
// - Tick 금지
// - Pawn/Character = Body(입력/코스메틱), SSOT = PlayerState
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesInteractionComponent.generated.h"

class AActor;
class APawn;
class APlayerController;

class AMosesFlagSpot;
class AMosesPickupWeapon;
class AMosesPickupAmmo;

class AMosesPlayerController;
class AMosesPlayerState;
class AMosesMatchGameState;

UENUM(BlueprintType)
enum class EMosesInteractTargetType : uint8
{
	None,
	FlagSpot,
	PickupWeapon,
	PickupAmmo,
};

USTRUCT(BlueprintType)
struct FMosesInteractTarget
{
	GENERATED_BODY()

	UPROPERTY()
	EMosesInteractTargetType Type = EMosesInteractTargetType::None;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;
};

UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesInteractionComponent();

	// =========================================================================
	// Client: Target Set/Clear (Overlap에서 호출)
	// =========================================================================
	void SetCurrentInteractTarget_Local(AActor* NewTarget);
	void ClearCurrentInteractTarget_Local(AActor* ExpectedTarget);

	// =========================================================================
	// Client: Input Endpoints (E Press/Release)
	// =========================================================================
	void RequestInteractPressed();
	void RequestInteractReleased();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// =========================================================================
	// RPC (Client -> Server)
	// =========================================================================
	UFUNCTION(Server, Reliable)
	void ServerInteractPressed(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerInteractReleased(AActor* TargetActor);

	// Server handlers (Authority only)
	void HandleInteractPressed_Server(AActor* TargetActor);
	void HandleInteractReleased_Server(AActor* TargetActor);

private:
	// =========================================================================
	// Guards / Helpers
	// =========================================================================
	bool IsWithinUseDistance_Server(const AActor* TargetActor) const;

	APawn* GetOwningPawn() const;
	APlayerController* GetOwningPC() const;

	AMosesPlayerState* GetMosesPlayerState() const;
	AMosesMatchGameState* GetMatchGameState() const;

	static EMosesInteractTargetType ResolveTargetType(AActor* TargetActor);

	// ✅ 서버에서 “획득자 PC”를 안전하게 해석 (OwnerOnly Toast)
	static AMosesPlayerController* ResolvePickerPC_Server(AMosesPlayerState* PS);

	// Target을 잃었거나 상태가 꼬였을 때 로컬 홀드 상태 강제 해제
	void ForceReleaseInteract_Local(AActor* ExpectedTarget, const TCHAR* ReasonTag);

private:
	// =========================================================================
	// Tunables (Server Guard)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float MaxUseDistance = 350.f;

private:
	// =========================================================================
	// Local cached target (Overlap set/clear)
	// =========================================================================
	UPROPERTY(Transient)
	FMosesInteractTarget CachedTarget;

	// E Hold state (Press once until Release)
	UPROPERTY(Transient)
	bool bHoldingInteract = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PressedTarget;
};
