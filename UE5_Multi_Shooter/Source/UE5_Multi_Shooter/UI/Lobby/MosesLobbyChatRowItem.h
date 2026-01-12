#pragma once

#include "UObject/Object.h"
#include "MosesLobbyChatRowItem.generated.h"

/**
 * UMosesLobbyChatRowItem
 * - UMG ListView에 들어갈 Item UObject
 * - BP EntryWidget에서 OnListItemObjectSet → 캐스팅 → Line 표시하면 됨
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyChatRowItem : public UObject
{
	GENERATED_BODY()

public:
	/** 위젯이 Outer가 되면 UI 수명과 같이 정리되어 안전 */
	static UMosesLobbyChatRowItem* Make(UObject* Outer, const FString& InLine)
	{
		UMosesLobbyChatRowItem* Item = NewObject<UMosesLobbyChatRowItem>(Outer);
		Item->Line = InLine;
		return Item;
	}

public:
	UPROPERTY()
	FString Line;
};
