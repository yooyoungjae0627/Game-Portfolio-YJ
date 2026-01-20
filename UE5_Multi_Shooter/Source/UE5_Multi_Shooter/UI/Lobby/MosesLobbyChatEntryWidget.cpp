#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatEntryWidget.h"

#include "Components/TextBlock.h"

void UMosesLobbyChatEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 개발자 주석:
	// - NickNameText/ChatText가 nullptr이면 WBP 바인딩 이름/IsVariable을 먼저 확인할 것.
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
		if (NickNameText)
		{
			NickNameText->SetText(FText::FromString(TEXT("Unknown")));
		}

		if (ChatText)
		{
			ChatText->SetText(FText::GetEmpty());
		}

		return;
	}

	if (NickNameText)
	{
		const FString Nick = Data->SenderName.TrimStartAndEnd().IsEmpty() ? TEXT("Guest") : Data->SenderName;
		NickNameText->SetText(FText::FromString(Nick));
	}

	if (ChatText)
	{
		ChatText->SetText(FText::FromString(Data->Message));
	}
}
