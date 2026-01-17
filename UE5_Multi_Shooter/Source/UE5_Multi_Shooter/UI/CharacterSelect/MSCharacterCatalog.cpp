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

bool UMSCharacterCatalog::FindByIndex(int32 InIndex, FMSCharacterEntry& OutEntry) const
{
	if (!Entries.IsValidIndex(InIndex))
	{
		return false;
	}

	OutEntry = Entries[InIndex];
	return true;
}
