#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "MosesHeroComponent.generated.h"

class UMosesCameraMode;

/**
 * UMosesHeroComponent (Simple)
 * - 로컬 플레이어 전용: 카메라 모드 델리게이트 바인딩 + 입력 초기화 훅
 * - Lyra InitState / GameFrameworkComponentManager 체인 제거 버전
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesHeroComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UMosesHeroComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;

private:
	/** PawnData -> DefaultCameraMode 반환 */
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

	/** 입력 초기화 훅 (지금은 뼈대만) */
	void InitializePlayerInput();

	/** 로컬 플레이어 여부 */
	bool IsLocalPlayerPawn() const;
};
