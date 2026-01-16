#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"

#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "GameFramework/Pawn.h"

UMosesPawnExtensionComponent::UMosesPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UMosesPawnExtensionComponent* UMosesPawnExtensionComponent::FindPawnExtensionComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UMosesPawnExtensionComponent>() : nullptr;
}

void UMosesPawnExtensionComponent::SetPawnData(const UMosesPawnData* InPawnData)
{
	// 단순 정책: 이미 있으면 유지
	if (PawnData)
	{
		return;
	}

	PawnData = InPawnData;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[PawnExt] SetPawnData Pawn=%s PawnData=%s"),
		*GetNameSafe(GetPawn<APawn>()),
		*GetNameSafe(PawnData));
}

void UMosesPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = GetPawn<APawn>();
	UE_LOG(LogMosesSpawn, Log, TEXT("[PawnExt] BeginPlay Pawn=%s PawnData=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(PawnData));
}
