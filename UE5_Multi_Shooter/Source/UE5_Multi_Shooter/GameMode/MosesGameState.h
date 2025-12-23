// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "MosesGameState.generated.h"

//class UMosesExperienceManagerComponent;

/**
 * 
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	AMosesGameState(const FObjectInitializer& ObjectInitializer);

	///** Experience 로딩/Ready를 담당 */
	//UPROPERTY()
	//TObjectPtr<UMosesExperienceManagerComponent> ExperienceManagerComponent;
	
};
