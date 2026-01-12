#include "CommandLexiconDA.h"

const TArray<FString>& UCommandLexiconDA::GetKeywords(EDialogueCommandType Type) const
{
	switch (Type)
	{
	case EDialogueCommandType::Pause:   return PauseKeywords;
	case EDialogueCommandType::Resume:  return ResumeKeywords;
	case EDialogueCommandType::Exit:    return ExitKeywords;
	case EDialogueCommandType::Skip:    return SkipKeywords;
	case EDialogueCommandType::Repeat:  return RepeatKeywords;
	case EDialogueCommandType::Restart: return RestartKeywords;
	default: break;
	}

	static const TArray<FString> Empty;
	return Empty;
}
