// ============================================================================
// MosesInteractionComponent.h (FULL)
// ----------------------------------------------------------------------------
// 역할
// - 로컬(클라)에서 카메라 기반으로 "현재 상호작용 대상"을 선택한다.
// - E Press/Release 입력을 받아 서버 RPC로 의도를 전달한다.
// - 서버는 거리/권한 등 최소 Guard를 수행한 뒤,
//   FlagSpot(Capture) 또는 PickupWeapon(Pickup)을 처리한다.
//
// 설계 원칙
// - Server Authority 100%: 결과 확정은 서버에서만.
// - Tick 금지: 대상 갱신은 Timer(UpdateInterval)로 수행.
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

UCLASS(ClassGroup=(Moses), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesInteractionComponent();

	// 클라 입력 엔드포인트(E Press/Release)
	void RequestInteractPressed();
	void RequestInteractReleased();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 로컬 대상 갱신 (Timer 기반)
	void UpdateInteractTarget_Local();
	FMosesInteractTarget FindBestTarget_Local() const;

	// 서버 RPC
	UFUNCTION(Server, Reliable)
	void ServerInteractPressed(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerInteractReleased(AActor* TargetActor);

	// 서버 처리
	void HandleInteractPressed_Server(AActor* TargetActor);
	void HandleInteractReleased_Server(AActor* TargetActor);

	// Guard
	bool IsWithinUseDistance_Server(const AActor* TargetActor) const;

	// Helpers
	APawn* GetOwningPawn() const;
	APlayerController* GetOwningPC() const;
	AMosesPlayerState* GetMosesPlayerState() const;

private:
	// Trace config (Local)
	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float TraceDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float UpdateInterval = 0.05f;

	// Server guard
	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float MaxUseDistance = 250.0f;

	FTimerHandle TimerHandle_UpdateTarget;

	UPROPERTY(Transient)
	FMosesInteractTarget CachedTarget;

	UPROPERTY(Transient)
	bool bHoldingInteract = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PressedTarget;
};
