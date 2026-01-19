#include "UE5_Multi_Shooter/Camera/MosesCameraMode_Sniper2x.h"
#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"

UMosesCameraMode_Sniper2x::UMosesCameraMode_Sniper2x(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 2배율 느낌: FOV 절반
	FieldOfView = Moses_CAMERA_DEFAULT_FOV * 0.5f;

	// 조준 전환 느낌(약간 부드럽게)
	BlendTime = 0.20f;
}
