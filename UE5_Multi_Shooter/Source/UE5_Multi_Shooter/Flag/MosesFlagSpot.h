// ============================================================================
// MosesFlagSpot.h (FULL)
// ----------------------------------------------------------------------------
// - Overlap 기반(존 안에서 E) FlagSpot
// - PromptWidget: 로컬 플레이어에게만 표시 + 항상 카메라를 바라봄(Billboard)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MosesFlagSpot.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

class APawn;
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

	// ---------------------------------------------------------------------
	// Server authority API
	// ---------------------------------------------------------------------
	bool ServerTryStartCapture(AMosesPlayerState* RequesterPS);
	void ServerCancelCapture(AMosesPlayerState* RequesterPS, EMosesCaptureCancelReason Reason);
	void SetFlagSystemEnabled(bool bEnable);

protected:
	virtual void BeginPlay() override;

private:
	// ---------------------------------------------------------------------
	// Internal flow (Server)
	// ---------------------------------------------------------------------
	void StartCapture_Internal(AMosesPlayerState* NewCapturerPS);
	void CancelCapture_Internal(EMosesCaptureCancelReason Reason);
	void FinishCapture_Internal();

	void TickCaptureProgress_Server();
	void ResetCaptureState_Server();

	// ---------------------------------------------------------------------
	// Overlap callbacks
	// ---------------------------------------------------------------------
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

	// ---------------------------------------------------------------------
	// Prompt helpers (Local only)
	// ---------------------------------------------------------------------
	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();
	bool IsLocalPawn(const APawn* Pawn) const;

	UMosesInteractionComponent* GetInteractionComponentFromPawn(APawn* Pawn) const;

	// [MOD] Billboard (Local only)
	void StartPromptBillboard_Local();
	void StopPromptBillboard_Local();
	void TickPromptBillboard_Local();
	void ApplyBillboardRotation_Local();

	// ---------------------------------------------------------------------
	// Guards
	// ---------------------------------------------------------------------
	bool CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const;
	bool IsCapturer_Server(const AMosesPlayerState* RequesterPS) const;
	bool IsDead_Server(const AMosesPlayerState* PS) const;

	float ResolveBroadcastSeconds() const;

private:
	// ---------------------------------------------------------------------
	// Components
	// ---------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<UStaticMeshComponent> SpotMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<USphereComponent> CaptureZone = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<UWidgetComponent> PromptWidgetComponent = nullptr;

private:
	// ---------------------------------------------------------------------
	// Tunables
	// ---------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Capture")
	float CaptureHoldSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Capture")
	float CaptureTickInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	TSoftObjectPtr<UMosesFlagFeedbackData> FeedbackData;

	// [MOD] Billboard tick (Local only)
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Prompt")
	float PromptBillboardInterval = 0.033f; // ~30fps

private:
	// ---------------------------------------------------------------------
	// Runtime (Server)
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	TObjectPtr<AMosesPlayerState> CapturerPS = nullptr;

	UPROPERTY(Transient)
	float CaptureElapsedSeconds = 0.0f;

	FTimerHandle TimerHandle_CaptureTick;

	UPROPERTY(Transient)
	bool bFlagSystemEnabled = true;

private:
	// ---------------------------------------------------------------------
	// Runtime (Client local)
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;

	FTimerHandle TimerHandle_PromptBillboard;
};
