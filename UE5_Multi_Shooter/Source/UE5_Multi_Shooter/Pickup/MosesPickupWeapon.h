// ============================================================================
// MosesPickupWeapon.h (FULL)  [MOD]
// - 월드에 배치되는 픽업 액터
// - 로컬 하이라이트(CustomDepth)는 클라 코스메틱.
// - 로컬 말풍선(WidgetComponent)도 클라 코스메틱. (서버 권위 아님)
// - 서버 원자성: bConsumed로 "OK 1 / FAIL 1" 보장.
// - 성공 시 PlayerState SSOT 갱신 -> GameState Announcement(RepNotify/Delegate) -> HUD 표시.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupWeapon.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

class UMosesPickupWeaponData;
class UMosesPickupPromptWidget;

class AMosesPlayerState;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupWeapon : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupWeapon();

	/** 클라: 로컬 하이라이트 On/Off (Overlap 등에서 호출) */
	void SetLocalHighlight(bool bEnable);

	/**
	 * 서버: 픽업 시도 (E)
	 * @param RequesterPS    픽업을 요청한 PlayerState(SSOT)
	 * @param OutAnnounceText 성공 시 중앙 Announcement에 표시할 텍스트
	 */
	bool ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText);

protected:
	virtual void BeginPlay() override;

private:
	// ---- Overlap for local highlight + prompt ----
	UFUNCTION()
	void HandleSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereEndOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// ---- Prompt helpers ----
	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();

	// ---- Guards ----
	bool CanPickup_Server(const AMosesPlayerState* RequesterPS) const;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere;

	/** [MOD] 월드 말풍선 UI(WidgetComponent) */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	// ---- Data ----
	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	TObjectPtr<UMosesPickupWeaponData> PickupData;

	// ---- Server atomic ----
	UPROPERTY()
	bool bConsumed = false;

	/** [MOD] 말풍선 표시 중인 로컬 Pawn (로컬 UX 대상 추적) */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;
};
