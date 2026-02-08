// MosesPerfMarker.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPerfMarker.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfMarker : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfMarker();

	FTransform GetMarkerTransform() const;

protected:
	virtual void BeginPlay() override;

private:
	// Marker ID must be fixed in level. (Marker01/02/03)
	UPROPERTY(EditInstanceOnly, Category = "Perf|Marker")
	FName MarkerId = NAME_None;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Marker")
	bool bUseActorRotation = true;

	UPROPERTY(EditInstanceOnly, Category = "Perf|Marker", meta = (EditCondition = "!bUseActorRotation"))
	FRotator FixedRotation = FRotator::ZeroRotator;

public:
	FName GetMarkerId() const { return MarkerId; }
};
