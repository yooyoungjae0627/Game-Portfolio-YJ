#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatRowItem.h"

UMosesLobbyChatRowItem* UMosesLobbyChatRowItem::Make(UObject* Outer, const FLobbyChatMessage& InMsg)
{
	UMosesLobbyChatRowItem* Item = NewObject<UMosesLobbyChatRowItem>(Outer);
	if (!Item)
	{
		return nullptr;
	}

	Item->SenderName = InMsg.SenderName;
	Item->SenderPid = InMsg.SenderPid;
	Item->Message = InMsg.Message;
	Item->ServerUnixTimeMs = InMsg.ServerUnixTimeMs;

	return Item;
}
