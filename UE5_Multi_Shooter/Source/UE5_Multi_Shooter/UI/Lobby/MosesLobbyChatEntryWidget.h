#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MosesLobbyChatEntryWidget.generated.h"

class UTextBlock;
class UMosesLobbyChatRowItem;

/**
 * UMosesLobbyChatEntryWidget
 * - ChatListView EntryWidget(C++)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyChatEntryWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	void ApplyChatData(const UMosesLobbyChatRowItem* Data);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NickNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ChatText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UMosesLobbyChatRowItem> CachedChatItem = nullptr;
};
