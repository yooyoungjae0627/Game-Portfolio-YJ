#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MSLobbyRoomItemWidget.generated.h"

class UTextBlock;
class USizeBox;

/**
 * ListView ItemData (Room)
 *
 * - ListView는 UObject 아이템을 받는 것이 정석이다.
 * - GameState의 FastArray(UStruct)를 UI에 직접 꽂지 말고,
 *   UI 전용 Data UObject로 변환해서 ListView에 넣는다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMSLobbyRoomListItemData : public UObject
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Functions
	// ---------------------------

	void Init(const FGuid& InRoomId, const int32 InMaxPlayers, const int32 InCurPlayers)
	{
		RoomId = InRoomId;
		MaxPlayers = InMaxPlayers;
		CurPlayers = InCurPlayers;
	}

	const FGuid& GetRoomId() const { return RoomId; }
	int32 GetMaxPlayers() const { return MaxPlayers; }
	int32 GetCurPlayers() const { return CurPlayers; }

private:
	// ---------------------------
	// Variables
	// ---------------------------

	UPROPERTY()
	FGuid RoomId;

	UPROPERTY()
	int32 MaxPlayers = 0;

	UPROPERTY()
	int32 CurPlayers = 0;
};


/**
 * Lobby Room ListView Entry Widget
 *
 * - RoomListView에 표시되는 "방 하나"의 UI 표현
 * - ListView가 넘겨주는 UObject(ItemData)를 받아 텍스트를 갱신한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMSLobbyRoomItemWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	// ---------------------------
	// Engine
	// ---------------------------
	virtual void NativeConstruct() override;

	// ---------------------------
	// IUserObjectListEntry
	// ---------------------------
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	// ---------------------------
	// Internal helpers
	// ---------------------------
	void ApplyRoomData(const UMSLobbyRoomListItemData* Data);

private:
	// ---------------------------
	// WBP BindWidget
	// ---------------------------
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_RoomTitle = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerCount = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerMaxCount = nullptr;

	// 개발자 주석:
	// - 지금 단계(RoomList)에서는 없어도 된다.
	// - WBP에 없을 가능성이 높으니 Optional로 둬서 바인딩 실패로 죽지 않게 한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NickNameText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChatText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> FirstBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SecondBox = nullptr;

	// ---------------------------
	// Cached
	// ---------------------------
	UPROPERTY(Transient)
	TObjectPtr<const UMSLobbyRoomListItemData> CachedRoomItem = nullptr;
};
