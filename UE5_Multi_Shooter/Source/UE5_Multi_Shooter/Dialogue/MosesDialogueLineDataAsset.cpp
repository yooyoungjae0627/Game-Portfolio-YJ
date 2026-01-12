#include "MosesDialogueLineDataAsset.h"

const FMosesDialogueLine* UMosesDialogueLineDataAsset::GetLine(int32 Index) const
{
	return Lines.IsValidIndex(Index) ? &Lines[Index] : nullptr;
}

int32 UMosesDialogueLineDataAsset::GetNumLines() const
{
	return Lines.Num();
}
