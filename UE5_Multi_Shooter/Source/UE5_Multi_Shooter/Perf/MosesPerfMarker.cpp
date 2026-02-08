// MosesPerfMarker.cpp
#include "UE5_Multi_Shooter/Perf/MosesPerfMarker.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "Engine/World.h"

AMosesPerfMarker::AMosesPerfMarker()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(false);
}

void AMosesPerfMarker::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* World = GetWorld();
	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][MARKER] BeginPlay MarkerId=%s Actor=%s Loc=%s Rot=%s World=%s NetMode=%d"),
		*MarkerId.ToString(),
		*GetNameSafe(this),
		*GetActorLocation().ToString(),
		*GetActorRotation().ToString(),
		*GetNameSafe(World),
		World ? (int32)World->GetNetMode() : -1);
}

FTransform AMosesPerfMarker::GetMarkerTransform() const
{
	const FRotator Rot = bUseActorRotation ? GetActorRotation() : FixedRotation;
	return FTransform(Rot, GetActorLocation(), FVector::OneVector);
}
