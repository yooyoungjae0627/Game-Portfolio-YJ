// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/GameInstance.h"
#include "MosesGameInstance.generated.h"

/**
 *
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesGameInstance : public UGameInstance
{
	GENERATED_BODY()

	/**
	 * UGameInstance's interfaces
	*/
	virtual void Init() override;
	virtual void Shutdown() override;


private:
	bool bDidServerBootLog = false;
	void TryLogServerBoot(UWorld* World, const UWorld::InitializationValues IVS);
};
