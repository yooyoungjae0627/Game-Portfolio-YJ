// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesPlayerCameraManager.h"

AMosesPlayerCameraManager::AMosesPlayerCameraManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultFOV = Moses_CAMERA_DEFAULT_FOV;
	ViewPitchMin = Moses_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = Moses_CAMERA_DEFAULT_PITCH_MAX;
}



