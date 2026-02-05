// MosesPerfMarker.cpp
#include "MosesPerfMarker.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

AMosesPerfMarker::AMosesPerfMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetActorEnableCollision(false);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][MARKER] Ctor Marker=%s"), *GetName());
}
