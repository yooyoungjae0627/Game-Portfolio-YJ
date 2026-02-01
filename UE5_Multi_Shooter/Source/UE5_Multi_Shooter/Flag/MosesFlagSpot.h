// ============================================================================
// MosesFlagSpot.h (FULL)
// ----------------------------------------------------------------------------
// 기획 변경 반영: "라인트레이스 조준" 제거, "오버랩 기반(존 안에서 E)"으로 단순화
//
// - CaptureZone 오버랩 시: 로컬 프롬프트 표시 + InteractionTarget 세팅
// - 존 안에서 E Press: 서버에 캡처 시작 요청
// - 존 이탈/Release/Dead/Damaged 시: 서버가 캡처 취소 확정
//
// 서버 권위/SSOT 원칙
// - 캡처 진행도/성공/취소 판정은 서버에서만 수행
// - PlayerState(SSOT)의 UMosesCaptureComponent에 상태를 기록/복제
//
// 네트워크 주의(중요)
// - InteractionComponent가 RPC로 Spot을 전달할 수 있도록 Spot은 Replicate ON 권장.
//   (Spot이 레벨 고정 6개라 비용은 사실상 무시 가능)
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
};
