// ============================================================================
// UE5_Multi_Shooter/Match/UI/Match/MosesResultPopupWidget.h  (NEW)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MosesResultPopupWidget.generated.h"

class UTextBlock;
class UButton;
class UBorder;

struct FMosesMatchResultState;

class AMosesPlayerState;

/**
 * UMosesResultPopupWidget
 *
 * 정책:
 * - Tick/Binding 금지
 * - ApplyResult(...) 한 번 호출해서 SetText 1회로 끝낸다.
 * - Confirm은 OwningPlayerController로 Server RPC 요청만 보낸다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesResultPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Result state + (MyPS, OppPS)로 UI를 한 번에 채운다.
	void ApplyResultOnce(
		const FMosesMatchResultState& ResultState,
		const AMosesPlayerState* MyPS,
		const AMosesPlayerState* OppPS);

protected:
	virtual void NativeConstruct() override;

private:
	// Confirm 버튼 클릭 처리
	UFUNCTION()
	void HandleConfirmClicked();

private:
	// -----------------------------
	// UI Widgets (BindWidget)
	// -----------------------------
	// 상단 상태 텍스트 (WINNER/LOSER/DRAW)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ResultState = nullptr;

	// 내 기록
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MyId = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MyCaptures = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MyZombieKills = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MyPvPKills = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MyTotalScore = nullptr;

	// 상대 기록
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_OppId = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_OppCaptures = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_OppZombieKills = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_OppPvPKills = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_OppTotalScore = nullptr;

	// Confirm 버튼
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm = nullptr;

private:
	// 내부 헬퍼
	static void SetText_Int(UTextBlock* TB, int32 Value);
	static void SetText_String(UTextBlock* TB, const FString& Str);
};
