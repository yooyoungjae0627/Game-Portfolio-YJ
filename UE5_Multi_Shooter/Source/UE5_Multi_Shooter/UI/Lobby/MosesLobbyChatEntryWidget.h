#pragma once

#include "UObject/Object.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MosesLobbyChatEntryWidget.generated.h"

class UTextBlock;

USTRUCT(BlueprintType)
struct FLobbyChatMessage
{
	GENERATED_BODY()

public:
	/** 표시용 송신자 닉네임 */
	UPROPERTY(BlueprintReadOnly)
	FString SenderName;

	/** 송신자 고유 PID (PlayerState PersistentId) */
	UPROPERTY(BlueprintReadOnly)
	FGuid SenderPid;

	/** 메시지 본문 */
	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** 서버 UTC UnixTime (ms) */
	UPROPERTY(BlueprintReadOnly)
	int64 ServerUnixTimeMs = 0;
};

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesLobbyChatRowItem : public UObject
{
	GENERATED_BODY()

public:
	/*====================================================
	= Factory
	====================================================*/

	/** 위젯이 Outer가 되면 UI 수명과 같이 정리되어 안전 */
	static UMosesLobbyChatRowItem* Make(UObject* Outer, const FLobbyChatMessage& InMsg)
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

public:
	/*====================================================
	= Data (EntryWidget에서 읽기 전용)
	====================================================*/

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString SenderName;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FGuid SenderPid;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	int64 ServerUnixTimeMs = 0;
};

/**
 * UMosesLobbyChatEntryWidget
 *
 * ChatListView의 EntryWidget(C++).
 *
 * 역할
 * - ListView가 전달한 ItemData(UObject)를 받아
 *   닉네임/메시지를 TextBlock에 각각 표시한다.
 *
 * WBP 요구사항(필수)
 * - 이 클래스를 Parent로 하는 WBP를 만들고
 *   TextBlock 이름을 정확히 맞춘다:
 *   - NickNameText
 *   - ChatText
 *
 * 주의
 * - BindWidget이므로, WBP에서 "Is Variable" 체크 + 이름 일치가 필수다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyChatEntryWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	/*====================================================
	= Engine
	====================================================*/
	virtual void NativeConstruct() override;

protected:
	/*====================================================
	= IUserObjectListEntry
	====================================================*/
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	/*====================================================
	= Internal helpers
	====================================================*/
	void ApplyChatData(const UMosesLobbyChatRowItem* Data);

private:
	/*====================================================
	= WBP BindWidget
	====================================================*/
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NickNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ChatText = nullptr;

private:
	/*====================================================
	= Cached
	====================================================*/
	UPROPERTY(Transient)
	TObjectPtr<const UMosesLobbyChatRowItem> CachedChatItem = nullptr;
};