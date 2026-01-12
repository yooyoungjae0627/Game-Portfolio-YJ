#pragma once

#include "Engine/DataAsset.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
#include "CommandLexiconDA.generated.h"

/**
 * UCommandLexiconDA
 * - 텍스트(STT/디버그) -> CommandType 판정을 위한 키워드 사전
 * - 키워드는 NormalizeText 기준(소문자/공백제거/특수제거)으로 저장 권장
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UCommandLexiconDA : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FString>& GetKeywords(EDialogueCommandType Type) const;

public:
	UPROPERTY(EditDefaultsOnly, Category="Lexicon") TArray<FString> PauseKeywords;
	UPROPERTY(EditDefaultsOnly, Category="Lexicon") TArray<FString> ResumeKeywords;
	UPROPERTY(EditDefaultsOnly, Category="Lexicon") TArray<FString> ExitKeywords;
	UPROPERTY(EditDefaultsOnly, Category="Lexicon") TArray<FString> SkipKeywords;
	UPROPERTY(EditDefaultsOnly, Category="Lexicon") TArray<FString> RepeatKeywords;
	UPROPERTY(EditDefaultsOnly, Category="Lexicon") TArray<FString> RestartKeywords;
};
