// ============================================================================
// MosesPickupPromptWidget.h (FULL)  [NEW]
// ----------------------------------------------------------------------------
// 역할
// - 월드 WidgetComponent에 붙는 "말풍선 프롬프트" 위젯의 C++ 래퍼.
// - Tick/Binding 금지: C++에서 SetText만 호출해서 갱신한다.
// - WBP_PickupPrompt는 이 클래스를 Parent로 사용하고,
//   TextBlock 이름을 아래 BindWidgetOptional과 동일하게 맞춘다.
//
// 사용 규칙
// - Show/Hide는 액터에서 WidgetComponent Visibility로 제어.
// - 텍스트는 SetPromptTexts로만 반영.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesPickupPromptWidget.generated.h"

class UTextBlock;

UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesPickupPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 프롬프트 텍스트를 갱신한다. (Tick/Binding 금지, SetText만 수행) */
	void SetPromptTexts(const FText& InInteractText, const FText& InItemNameText);

protected:
	virtual void NativeOnInitialized() override;

private:
	/** "E : 줍기" 같은 상호작용 안내 텍스트 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_Interact = nullptr;

	/** "Rifle" 같은 아이템 이름 텍스트 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_ItemName = nullptr;
};
