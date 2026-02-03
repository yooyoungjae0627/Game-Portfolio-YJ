// ============================================================================
// MosesFlagSpot.h (FULL)
// ----------------------------------------------------------------------------
// - Overlap 기반(존 안에서 E) FlagSpot
// - PromptWidget: 로컬 플레이어에게만 표시 + 항상 카메라를 바라봄(Billboard)
// - [DAY10][MOD] 캡처 성공(서버 확정) -> RespawnManager(10초 방송/0초 리스폰) 트리거
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

// [DAY10][MOD]
class AMosesZombieSpawnSpot;
class AMosesSpotRespawnManager;

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
	FORCEINLINE class USphereComponent* GetCaptureZone() const { return CaptureZone; }
	FORCEINLINE bool IsCapturing() const { return CapturerPS != nullptr; }

protected:
	virtual void BeginPlay() override;

protected:
	bool IsInsideCaptureZone_Server(const AMosesPlayerState* PS) const;

private:
	// ---------------------------------------------------------------------
	// Internal flow (Server)
	// ---------------------------------------------------------------------
	void StartCapture_Internal(AMosesPlayerState* NewCapturerPS);
	void CancelCapture_Internal(EMosesCaptureCancelReason Reason);
	void FinishCapture_Internal();

	void TickCaptureProgress_Server();
	void ResetCaptureState_Server();

	/** [MOD] 필요 시 서버에서만 Dormancy를 늦게 세팅한다 (초기 DormantAll 금지) */
	void ApplyDormancyPolicy_ServerOnly();

	// ---------------------------------------------------------------------
	// [DAY10][MOD] Capture Success -> Respawn Manager Trigger (Server)
	// ---------------------------------------------------------------------
	void NotifyRespawnManager_OnCaptured_Server();
	AMosesSpotRespawnManager* FindRespawnManager_Server() const;

private:
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
	/** [MOD] Dormancy를 정말 쓰고 싶다면, "초기 동기 이후"로 미룬다 */
	UPROPERTY(EditDefaultsOnly, Category = "Net|Dormancy", meta = (AllowPrivateAccess = "true"))
	bool bUseDormancyAfterBeginPlay = false;

	/** [MOD] Dormancy를 켠다면 어느 Dormancy로 갈지 */
	UPROPERTY(EditDefaultsOnly, Category = "Net|Dormancy", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ENetDormancy> DormancyAfterBeginPlay = DORM_DormantAll;

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
	// [DAY10][MOD] Level Link (Instance)
	// - 캡처 성공 시, 이 스팟에 연결된 좀비 스폰 스팟을 10초 후 리스폰한다.
	// ---------------------------------------------------------------------
	UPROPERTY(EditInstanceOnly, Category = "DAY10|Respawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AMosesZombieSpawnSpot> LinkedZombieSpawnSpot = nullptr;

	/** RespawnManager를 명시하고 싶으면 지정. 비우면 월드에서 자동 탐색 */
	UPROPERTY(EditInstanceOnly, Category = "DAY10|Respawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AMosesSpotRespawnManager> RespawnManagerOverride = nullptr;

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
