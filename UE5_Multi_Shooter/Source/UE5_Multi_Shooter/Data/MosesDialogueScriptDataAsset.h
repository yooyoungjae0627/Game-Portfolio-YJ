#pragma once

#include "Engine/DataAsset.h"
#include "UE5_Multi_Shooter/MosesDialogueTypes.h"
#include "MosesDialogueScriptDataAsset.generated.h"

/**
 * 대사 스크립트 DataAsset
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesDialogueScriptDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMosesDialogueLine> Lines;
};
