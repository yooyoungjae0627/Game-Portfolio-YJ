// MosesPerfMarker.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosesPerfTypes.h"
#include "MosesPerfMarker.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPerfMarker : public AActor
{
	GENERATED_BODY()

public:
	AMosesPerfMarker();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moses|Perf")
	EMosesPerfMarkerId MarkerId = EMosesPerfMarkerId::Marker01;

	/** 카메라를 이 Transform으로 고정하고 캡처 수행 */
	UFUNCTION(BlueprintPure, Category="Moses|Perf")
	FTransform GetMarkerTransform() const { return GetActorTransform(); }
};
