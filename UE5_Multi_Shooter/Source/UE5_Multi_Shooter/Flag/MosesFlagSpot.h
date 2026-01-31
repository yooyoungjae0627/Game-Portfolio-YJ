// ============================================================================
// MosesFlagSpot.h (FULL)
// - Flag Spot Actor
// - 서버 권위로 캡처(3초 유지) 진행/취소/성공을 확정한다.
// - 진행도는 캡처 중인 PlayerState의 UMosesCaptureComponent(SSOT)에만 기록한다.
// - 캡처 취소 사유(이탈/사망/피격/Release 등)는 서버가 확정한다.
// - HUD는 PlayerState(SSOT) RepNotify -> Delegate로만 갱신한다.
// - [UPDATED] GF_Match_Rules가 캡처를 Enable/Disable 할 수 있도록 Gate 추가
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesFlagSpot.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AMosesPlayerState;

UENUM(BlueprintType)
enum class EMosesCaptureCancelReason : uint8
{
	None,
	Released,
	LeftZone,
	Dead,
	Damaged,
	ServerRejected,
	SystemDisabled // [UPDATED] GF로 비활성화됨
};

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesFlagSpot : public AActor
{
	GENERATED_BODY()

public:
	AMosesFlagSpot();

	// 서버: 캡처 시작 요청(Press)
	bool ServerTryStartCapture(AMosesPlayerState* RequesterPS);

	// 서버: 캡처 취소 요청(Release 등)
	void ServerCancelCapture(AMosesPlayerState* RequesterPS, EMosesCaptureCancelReason Reason);

	// [UPDATED] GF 룰 스위치가 서버에서 호출
	void SetFlagSystemEnabled(bool bEnable);

protected:
	virtual void BeginPlay() override;

private:
	// ---- Internal flow ----
	void StartCapture_Internal(AMosesPlayerState* NewCapturerPS);
	void CancelCapture_Internal(EMosesCaptureCancelReason Reason);
	void FinishCapture_Internal();

	void TickCaptureProgress_Server();
	void ResetCaptureState_Server();

	// ---- Overlap callbacks ----
	UFUNCTION()
	void HandleZoneBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleZoneEndOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// ---- Guards ----
	bool CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const;
	bool IsCapturer_Server(const AMosesPlayerState* RequesterPS) const;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SpotMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CaptureZone;

	// ---- Tunables ----
	UPROPERTY(EditDefaultsOnly, Category="Flag|Capture")
	float CaptureHoldSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Flag|Capture")
	float CaptureTickInterval = 0.05f;

	// ---- Server state ----
	UPROPERTY()
	TObjectPtr<AMosesPlayerState> CapturerPS;

	float CaptureElapsedSeconds = 0.0f;

	FTimerHandle TimerHandle_CaptureTick;

	// [UPDATED] GF 룰 스위치 Gate
	UPROPERTY()
	bool bFlagSystemEnabled = true;
};
