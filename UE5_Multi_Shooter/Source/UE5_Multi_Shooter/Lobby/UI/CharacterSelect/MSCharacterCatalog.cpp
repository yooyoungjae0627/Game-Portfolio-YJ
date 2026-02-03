#include "UE5_Multi_Shooter/Lobby/UI/CharacterSelect/MSCharacterCatalog.h"

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
	// SelectedCharacterId´Â 1-base(1..N)
	const int32 Index0 = InSelectedId - 1;

	if (!Entries.IsValidIndex(Index0))
	{
		return false;
	}

	OutEntry = Entries[Index0];
	return true;
}
