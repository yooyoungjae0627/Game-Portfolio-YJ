#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h" 
#include "MosesPawnData.generated.h"

class APawn;
class UMosesCameraMode;

/**
 * UMosesPawnData
 *
 * [한 줄 역할]
 * - “이 Experience에서 사용할 Pawn의 구성(클래스/카메라)”을 데이터로 정의한다.
 *
 * [포트폴리오 의도]
 * - 서버는 Experience를 고르고,
 * - Experience는 PawnData를 고르고,
 * - PawnData가 PawnClass를 결정한다.
 * => 즉, 스폰이 데이터 기반으로 바뀌어 Lyra 스타일이 된다.
 */
UCLASS(BlueprintType, Const)
class UE5_MULTI_SHOOTER_API UMosesPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMosesPawnData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	/** 이 PawnData가 가리키는 Pawn 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Pawn")
	TSubclassOf<APawn> PawnClass;

	/** 기본 카메라 모드(네가 이미 만든 MosesCameraMode 시스템과 연결) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Camera")
	TSubclassOf<UMosesCameraMode> DefaultCameraMode;
};
