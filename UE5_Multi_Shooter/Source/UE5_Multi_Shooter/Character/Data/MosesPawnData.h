// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"
#include "MosesPawnData.generated.h"

//class UMosesInputConfig;
class UMosesCameraMode;

/**
 * 
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UMosesPawnData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Pawn¿« Class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "YJ|Pawn")
	TSubclassOf<APawn> PawnClass;

	///** Camera Mode */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "YJ|Camera")
	TSubclassOf<UMosesCameraMode> DefaultCameraMode;

	/** input configuration used by player controlled pawns to create input mappings and bind input actions */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "YJ|InputConfig")
	//TObjectPtr<UYJInputConfig> InputConfig;
	
	
};
