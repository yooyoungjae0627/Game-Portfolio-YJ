// ============================================================================
// MosesInteractionComponent.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// 역할
// - [MOD] "Overlap 기반" 상호작용 타겟 시스템.
//   * LineTrace/Timer로 타겟을 찾지 않는다.
//   * PickupWeapon/FlagSpot이 Overlap(Begin/End) 시 로컬 플레이어의 이 컴포넌트에
//     Target을 Set/Clear 해준다.
// - E Press/Release 입력을 받아 서버 RPC로 의도를 전달한다.
// - 서버는 거리/권한 등 최소 Guard를 수행한 뒤,
//   FlagSpot(Capture) 또는 PickupWeapon(Pickup)을 처리한다.
//
// 설계 원칙
// - Server Authority 100%: 결과 확정은 서버에서만.
// - Tick 금지: 타겟 갱신은 Overlap 이벤트로만 수행한다.
// - Pawn/Character는 Body(입력/코스메틱), SSOT는 PlayerState.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesInteractionComponent.generated.h"

class AMosesFlagSpot;
class AMosesPickupWeapon;
class AMosesPlayerState;
class AMosesMatchGameState;

UENUM()
enum class EMosesInteractTargetType : uint8
{
	None,
	FlagSpot,
	Pickup
};

USTRUCT()
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

	/**
	 * [MOD] 로컬 플레이어가 Overlap 진입했을 때 타겟을 설정한다.
	 * - 호출 주체: PickupWeapon / FlagSpot (Overlap Begin)
	 * - 주의: 로컬 플레이어에서만 호출되어야 한다.
	 */
	void SetCurrentInteractTarget_Local(AActor* NewTarget);

	/**
	 * [MOD] 로컬 플레이어가 Overlap 이탈했을 때 타겟을 해제한다.
	 * - ExpectedTarget이 현재 타겟과 같을 때만 해제한다.
	 */
	void ClearCurrentInteractTarget_Local(AActor* ExpectedTarget);

	// =========================================================================
	// Client Input Endpoints (E Press/Release)
	// =========================================================================
	void RequestInteractPressed();
	void RequestInteractReleased();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// =========================================================================
	// Server RPC
	// =========================================================================
	UFUNCTION(Server, Reliable)
	void ServerInteractPressed(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerInteractReleased(AActor* TargetActor);

	// Server handlers
	void HandleInteractPressed_Server(AActor* TargetActor);
	void HandleInteractReleased_Server(AActor* TargetActor);

	// Guard
	bool IsWithinUseDistance_Server(const AActor* TargetActor) const;

	// Helpers
	APawn* GetOwningPawn() const;
	APlayerController* GetOwningPC() const;
	AMosesPlayerState* GetMosesPlayerState() const;
	AMosesMatchGameState* GetMatchGameState() const;

	// [MOD] Target type resolve
	static EMosesInteractTargetType ResolveTargetType(AActor* TargetActor);

private:
	// -------------------------------------------------------------------------
	// Server guard
	// -------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float MaxUseDistance = 250.0f;

	// -------------------------------------------------------------------------
	// [MOD] Cached target set by overlap (no timer, no trace)
	// -------------------------------------------------------------------------
	UPROPERTY(Transient)
	FMosesInteractTarget CachedTarget;

	// E Hold state (Press once until Release)
	UPROPERTY(Transient)
	bool bHoldingInteract = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PressedTarget;
};
