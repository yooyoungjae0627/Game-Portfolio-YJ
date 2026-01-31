// ============================================================================
// MosesCaptureComponent.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// Owner: PlayerState(SSOT)
// - 캡처 진행도/상태를 RepNotify로 복제하고 Native Delegate로 HUD를 갱신한다.
// - FlagSpot 서버가 이 컴포넌트의 값을 "서버에서만" 변경한다.
//
// 주의(중요)
// - FlagSpot은 레벨 액터(비복제)로 운용할 수 있다.
// - 따라서 CaptureState.Spot은 클라에서 null일 수 있으며,
//   HUD는 Spot 포인터가 없어도 진행바 표시가 가능해야 한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesCaptureComponent.generated.h"

class AMosesFlagSpot;

DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnMosesCaptureStateChangedNative,
	bool /*bCapturing*/,
	float /*ProgressAlpha*/,
	float /*HoldSeconds*/,
	TWeakObjectPtr<AMosesFlagSpot> /*Spot*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnMosesCapturesChangedNative,
	int32 /*NewCaptures*/,
	float /*BestCaptureTime*/);

USTRUCT()
struct FMosesCaptureState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bCapturing = false;

	// 0..1
	UPROPERTY()
	float ProgressAlpha = 0.0f;

	UPROPERTY()
	float HoldSeconds = 3.0f;

	// [MOD] Spot은 클라에서 null일 수 있다(FlagSpot 비복제일 수 있음).
	UPROPERTY()
	TWeakObjectPtr<AMosesFlagSpot> Spot;
};

UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCaptureComponent();

	// 서버: FlagSpot이 캡처 상태/진행도를 갱신
	void ServerSetCapturing(AMosesFlagSpot* Spot, bool bNewCapturing, float NewProgressAlpha, float HoldSeconds);

	// 서버: 캡처 성공 확정(점수/기록)
	void ServerOnCaptureSucceeded(AMosesFlagSpot* Spot, float CaptureTimeSeconds);

	// HUD 바인딩용 Native Delegates
	FOnMosesCaptureStateChangedNative OnCaptureStateChanged;
	FOnMosesCapturesChangedNative OnCapturesChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ---- RepNotifies ----
	UFUNCTION()
	void OnRep_CaptureState();

	UFUNCTION()
	void OnRep_Captures();

	// ---- Internal helpers ----
	void BroadcastCaptureState();
	void BroadcastCaptures();

private:
	// ---- SSOT replicated state ----
	UPROPERTY(ReplicatedUsing = OnRep_CaptureState)
	FMosesCaptureState CaptureState;

	UPROPERTY(ReplicatedUsing = OnRep_Captures)
	int32 Captures = 0;

	// [MOD] BestCaptureTimeSeconds는 Captures와 함께 HUD에 표시되는 값이므로
	//       클라도 값 변경을 관찰할 수 있게 Replicated 유지.
	UPROPERTY(Replicated)
	float BestCaptureTimeSeconds = 0.0f; // 낮을수록 좋음
};
