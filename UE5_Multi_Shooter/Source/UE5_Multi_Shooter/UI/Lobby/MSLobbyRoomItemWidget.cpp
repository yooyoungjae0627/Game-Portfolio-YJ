#include "MSLobbyRoomItemWidget.h"

#include "Components/TextBlock.h"
#include "Components/SizeBox.h"

void UMSLobbyRoomItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 개발자 주석:
	// - BindWidget 실패는 대부분 WBP에서 "Is Variable" 체크 누락 또는 이름 불일치.
}

void UMSLobbyRoomItemWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UMSLobbyRoomListItemData* Data = Cast<UMSLobbyRoomListItemData>(ListItemObject);
	CachedRoomItem = Data;

	ApplyRoomData(Data);
}

void UMSLobbyRoomItemWidget::ApplyRoomData(const UMSLobbyRoomListItemData* Data)
{
	if (!Data)
	{
		if (Text_RoomTitle) { Text_RoomTitle->SetText(FText::FromString(TEXT("Invalid Room"))); }
		if (Text_PlayerCount) { Text_PlayerCount->SetText(FText::FromString(TEXT("0"))); }
		if (Text_PlayerMaxCount) { Text_PlayerMaxCount->SetText(FText::FromString(TEXT("0"))); }
		return;
	}

	// 개발자 주석:
	// - 방 제목은 아직 서버 데이터에 없으니 RoomId 일부로 임시 표시
	if (Text_RoomTitle)
	{
		const FString Title = FString::Printf(TEXT("ROOM_%s"), *Data->GetRoomId().ToString(EGuidFormats::Short));
		Text_RoomTitle->SetText(FText::FromString(Title));
	}

	if (Text_PlayerCount)
	{
		Text_PlayerCount->SetText(FText::AsNumber(Data->GetCurPlayers()));
	}

	if (Text_PlayerMaxCount)
	{
		Text_PlayerMaxCount->SetText(FText::AsNumber(Data->GetMaxPlayers()));
	}
}
