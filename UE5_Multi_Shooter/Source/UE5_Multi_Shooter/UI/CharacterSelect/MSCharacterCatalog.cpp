// Fill out your copyright notice in the Description page of Project Settings.

#include "MSCharacterCatalog.h"

bool UMSCharacterCatalog::FindById(const FName InId, FMSCharacterEntry& OutEntry) const
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


