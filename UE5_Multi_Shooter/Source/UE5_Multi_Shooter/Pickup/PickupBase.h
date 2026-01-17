#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APawn;

UCLASS()
class UE5_MULTI_SHOOTER_API APickupBase : public AActor
{
	GENERATED_BODY()

public:
	APickupBase();

	// ---------------------------
	// Engine
	// ---------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------------------------
	// Interaction (Server authoritative)
	// ---------------------------

	bool IsConsumed() const { return bConsumed; }

	/** 서버에서만 호출: 원자성 보장 + 보상 적용 + Destroy */
	bool Server_TryConsume(APawn* InstigatorPawn);

protected:
	/** 서버에서만 호출: 보상 적용(파생 클래스에서 구현) */
	virtual bool Server_ApplyPickup(APawn* InstigatorPawn);

private:
	// ---------------------------
	// Security / Validation
	// ---------------------------

	bool Server_ValidateConsume(APawn* InstigatorPawn, FString& OutFailReason) const;

	float GetDistanceToPawn(APawn* InstigatorPawn) const;

	UFUNCTION()
	void OnRep_Consumed();

private:
	// ---------------------------
	// Components
	// ---------------------------

	UPROPERTY(VisibleAnywhere, Category="Pickup")
	TObjectPtr<USphereComponent> SphereCollision;

	UPROPERTY(VisibleAnywhere, Category="Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	// ---------------------------
	// Replicated State (Atomic)
	// ---------------------------

	UPROPERTY(ReplicatedUsing=OnRep_Consumed)
	bool bConsumed = false;

	/** 서버 전용 재진입 가드 */
	bool bProcessing = false;

private:
	// ---------------------------
	// Tuning
	// ---------------------------

	UPROPERTY(EditDefaultsOnly, Category="Pickup|Tuning")
	float InteractionRange = 250.0f;
};
