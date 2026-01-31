// ============================================================================
// MosesFlagSpot.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// [MOD 핵심]
// - Overlap 기반 타겟 시스템 적용:
//   * CaptureZone BeginOverlap(클라 로컬)에서 InteractionComponent에 Target Set
//   * CaptureZone EndOverlap(클라 로컬)에서 Target Clear
// - "존 안에서 E" 로 캡처가 시작되도록 UX가 단순화된다.
// - 서버 로직(3초 유지/취소/성공/Announcement)은 기존 구조 유지.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesFlagSpot.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

class AMosesPlayerState;
class UMosesFlagFeedbackData;
class UMosesInteractionComponent;

UENUM(BlueprintType)
enum class EMosesCaptureCancelReason : uint8
{
	None,
	Released,
	LeftZone,
	Dead,
	Damaged,
	ServerRejected,
	SystemDisabled
};

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesFlagSpot : public AActor
{
	GENERATED_BODY()

public:
	AMosesFlagSpot();

	bool ServerTryStartCapture(AMosesPlayerState* RequesterPS);
	void ServerCancelCapture(AMosesPlayerState* RequesterPS, EMosesCaptureCancelReason Reason);
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

	// ---- Prompt helpers (Local only) ----
	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();
	bool IsLocalPawn(const APawn* Pawn) const;

	// [MOD] interaction component resolve
	UMosesInteractionComponent* GetInteractionComponentFromPawn(APawn* Pawn) const;

	// ---- Guards ----
	bool CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const;
	bool IsCapturer_Server(const AMosesPlayerState* RequesterPS) const;

	bool IsDead_Server(const AMosesPlayerState* PS) const;
	float ResolveBroadcastSeconds() const;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SpotMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CaptureZone;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Capture")
	float CaptureHoldSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Capture")
	float CaptureTickInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	TSoftObjectPtr<UMosesFlagFeedbackData> FeedbackData;

	UPROPERTY()
	TObjectPtr<AMosesPlayerState> CapturerPS;

	float CaptureElapsedSeconds = 0.0f;

	FTimerHandle TimerHandle_CaptureTick;

	UPROPERTY()
	bool bFlagSystemEnabled = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;
};
