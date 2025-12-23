// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/PlayerCameraManager.h"
#include "MosesPlayerCameraManager.generated.h"

/**
 * 
 */

#define Moses_CAMERA_DEFAULT_FOV (80.0f)
#define Moses_CAMERA_DEFAULT_PITCH_MIN (-89.0f)
#define Moses_CAMERA_DEFAULT_PITCH_MAX (89.0f)


UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:
	AMosesPlayerCameraManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
};
