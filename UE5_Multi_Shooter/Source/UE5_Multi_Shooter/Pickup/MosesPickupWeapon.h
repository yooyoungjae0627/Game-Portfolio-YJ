// ============================================================================
// MosesPickupWeapon.h (FULL)  [MOD + Billboard]
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupWeapon.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UWidgetComponent;

class UMosesPickupWeaponData;
class UMosesPickupPromptWidget;
class AMosesPlayerState;
class UMosesInteractionComponent;
class APawn;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupWeapon : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupWeapon();

	void SetLocalHighlight(bool bEnable);
	bool ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText);

protected:
	virtual void BeginPlay() override;

private:
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

	void SetPromptVisible_Local(bool bVisible);
	void ApplyPromptText_Local();

	bool CanPickup_Server(const AMosesPlayerState* RequesterPS) const;
	UMosesInteractionComponent* GetInteractionComponentFromPawn(APawn* Pawn) const;

	// [MOD] Billboard (Local only)
	void StartPromptBillboard_Local();
	void StopPromptBillboard_Local();
	void TickPromptBillboard_Local();
	void ApplyBillboardRotation_Local();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractSphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	TObjectPtr<UMosesPickupWeaponData> PickupData = nullptr;

	UPROPERTY()
	bool bConsumed = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> LocalPromptPawn;

	// [MOD] Billboard tick (Local only)
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Prompt")
	float PromptBillboardInterval = 0.033f; // ~30fps

	FTimerHandle TimerHandle_PromptBillboard;
};
