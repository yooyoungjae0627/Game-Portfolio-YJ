#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "MosesPlayerCameraManager.generated.h"

/**
 * AMosesPlayerCameraManager
 *
 * [기능]
 * - 프로젝트 기본 FOV, Pitch 제한값을 고정하는 용도.
 * - 실제 카메라 모드 전환/블렌딩은 UMosesCameraComponent + CameraModeStack이 담당한다.
 *
 * [명세]
 * - DefaultFOV / ViewPitchMin/Max를 기본값으로 세팅한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AMosesPlayerCameraManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

#define Moses_CAMERA_DEFAULT_FOV (80.0f)
#define Moses_CAMERA_DEFAULT_PITCH_MIN (-89.0f)
#define Moses_CAMERA_DEFAULT_PITCH_MAX (89.0f)
