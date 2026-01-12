#pragma once

#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
#include "MosesDialogueCommandDetector.generated.h"

class UCommandLexiconDA;

USTRUCT()
struct FDialogueDetectResult
{
	GENERATED_BODY()

	UPROPERTY() EDialogueCommandType Type = EDialogueCommandType::None;
	UPROPERTY() FString MatchedKeyword;
	UPROPERTY() FString NormalizedText;
};

/**
 * UDialogueCommandDetector
 * - Normalize 후 Lexicon 키워드 포함 여부로 명령 판정
 * - 우선순위 고정: Pause > Resume > Exit > Skip > Repeat > Restart
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesDialogueCommandDetector : public UObject
{
	GENERATED_BODY()

public:
	static FString NormalizeText(const FString& In);
	static FDialogueDetectResult DetectIntent(const FString& InText, const UCommandLexiconDA* Lexicon);
};
