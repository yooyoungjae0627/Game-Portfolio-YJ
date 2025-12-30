// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

void AMosesGameState::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogMosesExp, Warning, TEXT("[GS] BeginPAMosesPlayerControlleray World=%s NetMode=%d"),
		*GetNameSafe(GetWorld()), (int32)GetNetMode());
}

AMosesGameState::AMosesGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	 ExperienceManagerComponent = CreateDefaultSubobject<UMosesExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}



