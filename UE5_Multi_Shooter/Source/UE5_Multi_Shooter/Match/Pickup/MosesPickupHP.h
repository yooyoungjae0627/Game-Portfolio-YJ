#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPickupHP.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UGameplayEffect;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPickupHP : public AActor
{
	GENERATED_BODY()

public:
	AMosesPickupHP();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	void HandleOverlap_Server(AActor* OtherActor);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

public:
	UPROPERTY(EditAnywhere, Category="Moses|HP")
	int32 HealMin = 10;

	UPROPERTY(EditAnywhere, Category="Moses|HP")
	int32 HealMax = 30;

	UPROPERTY(EditDefaultsOnly, Category="Moses|GAS")
	TSubclassOf<UGameplayEffect> GE_Heal_SetByCaller;
};
