#include "MosesDialogueCommandDetector.h"
#include "CommandLexiconDA.h"

static bool ContainsAnyKeyword(const FString& Normalized, const TArray<FString>& Keywords, FString& OutMatched)
{
	for (const FString& K : Keywords)
	{
		if (!K.IsEmpty() && Normalized.Contains(K))
		{
			OutMatched = K;
			return true;
		}
	}
	return false;
}

FString UMosesDialogueCommandDetector::NormalizeText(const FString& In)
{
	FString Out = In.ToLower();
	Out.ReplaceInline(TEXT(" "), TEXT(""));

	const TCHAR* RemoveChars = TEXT(".,!?~@#$%^&*()[]{}<>/\\|\"':;`");
	for (const TCHAR* P = RemoveChars; *P; ++P)
	{
		FString C; C.AppendChar(*P);
		Out.ReplaceInline(*C, TEXT(""));
	}

	return Out;
}

FDialogueDetectResult UMosesDialogueCommandDetector::DetectIntent(const FString& InText, const UCommandLexiconDA* Lexicon)
{
	FDialogueDetectResult R;
	R.NormalizedText = NormalizeText(InText);

	if (!Lexicon)
	{
		return R;
	}

	// 우선순위 고정(정책)
	const EDialogueCommandType Priority[] =
	{
		EDialogueCommandType::Pause,
		EDialogueCommandType::Resume,
		EDialogueCommandType::Exit,
		EDialogueCommandType::Skip,
		EDialogueCommandType::Repeat,
		EDialogueCommandType::Restart,
	};

	for (const EDialogueCommandType Type : Priority)
	{
		FString Matched;
		if (ContainsAnyKeyword(R.NormalizedText, Lexicon->GetKeywords(Type), Matched))
		{
			R.Type = Type;
			R.MatchedKeyword = Matched;
			return R;
		}
	}

	return R;
}
