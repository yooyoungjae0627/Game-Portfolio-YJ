// ============================================================================
// MosesFlagSpot.h (FULL)  [MOD]
// ----------------------------------------------------------------------------
// Flag Spot Actor
//
// 서버 권위로 캡처(3초 유지) 진행/취소/성공을 확정한다.
// 진행도는 캡처 중인 PlayerState의 UMosesCaptureComponent(SSOT)에만 기록한다.
// 캡처 취소 사유(이탈/사망/Release 등)는 서버가 확정한다.
// HUD는 PlayerState(SSOT) RepNotify -> Delegate로만 갱신한다.
//
// [MOD]
// - LineTrace(ECC_Visibility)로 FlagSpot을 선택할 수 있도록 TraceTarget(UBoxComponent) 추가.
// - 월드 말풍선 프롬프트(WidgetComponent) 추가: 로컬 UX.
// - 성공 방송을 MosesMatchGameState Announcement로 통일.
// - DeadReject를 PS 소유 CombatComponent(IsDead)로 판정.
// - FeedbackData(DA)로 BroadcastDefaultSeconds 관리.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesFlagSpot.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;

class AMosesPlayerState;
class UMosesFlagFeedbackData;

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

	// 서버: 캡처 시작 요청(Press)
	bool ServerTryStartCapture(AMosesPlayerState* RequesterPS);

	// 서버: 캡처 취소 요청(Release 등)
	void ServerCancelCapture(AMosesPlayerState* RequesterPS, EMosesCaptureCancelReason Reason);

	// GF 룰 스위치가 서버에서 호출
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

	// ---- Overlap callbacks (서버: 존 관리 / 클라: 프롬프트) ----
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

	// ---- Guards ----
	bool CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const;
	bool IsCapturer_Server(const AMosesPlayerState* RequesterPS) const;

	// [MOD] Dead 판정(SSOT)
	bool IsDead_Server(const AMosesPlayerState* PS) const;

	// [MOD] Announcement duration resolve
	float ResolveBroadcastSeconds() const;

private:
	// ---- Components ----
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SpotMesh;

	// 캡처 존(Overlap): 서버는 이탈 취소, 클라는 프롬프트 표시 트리거로도 사용
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CaptureZone;

	// [MOD] Trace용 타겟(Visibility Block) - InteractionComponent LineTrace가 여기를 히트한다.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> TraceTarget;

	// [MOD] 월드 말풍선 프롬프트(UI) - 로컬만 Visible
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	// ---- Tunables ----
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Capture")
	float CaptureHoldSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Flag|Capture")
	float CaptureTickInterval = 0.05f;

	// [MOD] FeedbackData (DataAsset)
	UPROPERTY(EditDefaultsOnly, Category = "Flag|Feedback")
	TSoftObjectPtr<UMosesFlagFeedbackData> FeedbackData;

	// ---- Server state ----
	UPROPERTY()
	TObjectPtr<AMosesPlayerState> CapturerPS;

	float CaptureElapsedSeconds = 0.0f;

	FTimerHandle TimerHandle_CaptureTick;

	// GF 룰 스위치 Gate
	UPROPERTY()
	bool bFlagSystemEnabled = true;

	// [MOD] 로컬 프롬프트 대상 추적
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;
};
