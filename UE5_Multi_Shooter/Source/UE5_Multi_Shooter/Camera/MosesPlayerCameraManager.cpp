#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"

AMosesPlayerCameraManager::AMosesPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 프로젝트 기본값 고정
	DefaultFOV = Moses_CAMERA_DEFAULT_FOV;
	ViewPitchMin = Moses_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = Moses_CAMERA_DEFAULT_PITCH_MAX;
}


