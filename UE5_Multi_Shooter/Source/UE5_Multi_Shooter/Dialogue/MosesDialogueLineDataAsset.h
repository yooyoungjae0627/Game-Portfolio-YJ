#pragma once

#include "Engine/DataAsset.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
#include "Sound/SoundBase.h"
#include "MosesDialogueLineDataAsset.generated.h"


/**
 * Dialogue 스크립트 묶음
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesDialogueLineDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Accessors
	// ---------------------------
	const FMosesDialogueLine* GetLine(int32 Index) const;
	int32 GetNumLines() const;

private:
	// ---------------------------
	// Data
	// ---------------------------
	UPROPERTY(EditAnywhere)
	TArray<FMosesDialogueLine> Lines;
};
