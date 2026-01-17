#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"

bool UMSCharacterCatalog::FindById(FName InId, FMSCharacterEntry& OutEntry) const
{
	for (const FMSCharacterEntry& Entry : Entries)
	{
		if (Entry.CharacterId == InId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}

bool UMSCharacterCatalog::FindByIndex(int32 InSelectedId, FMSCharacterEntry& OutEntry) const
{
	// [FIX] SelectedCharacterId는 1-base(1..N)로 들어온다.
	//       Entries는 0-base 배열이므로 변환이 필요하다.
	const int32 Index0 = InSelectedId - 1;

	if (!Entries.IsValidIndex(Index0))
	{
		return false;
	}

	OutEntry = Entries[Index0];
	return true;
}
