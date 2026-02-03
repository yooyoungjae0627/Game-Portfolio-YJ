#include "UE5_Multi_Shooter/Lobby/UI/MosesLobbyChatEntryWidget.h"
#include "UE5_Multi_Shooter/Lobby/UI/MosesLobbyChatRowItem.h"

#include "Components/TextBlock.h"

void UMosesLobbyChatEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMosesLobbyChatEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UMosesLobbyChatRowItem* Data = Cast<UMosesLobbyChatRowItem>(ListItemObject);
	CachedChatItem = Data;

	ApplyChatData(Data);
}

void UMosesLobbyChatEntryWidget::ApplyChatData(const UMosesLobbyChatRowItem* Data)
{
	if (!Data)
	{
		if (NickNameText) { NickNameText->SetText(FText::FromString(TEXT("Unknown"))); }
		if (ChatText) { ChatText->SetText(FText::GetEmpty()); }
		return;
	}

	if (NickNameText) { NickNameText->SetText(FText::FromString(Data->SenderName)); }
	if (ChatText) { ChatText->SetText(FText::FromString(Data->Message)); }
}
