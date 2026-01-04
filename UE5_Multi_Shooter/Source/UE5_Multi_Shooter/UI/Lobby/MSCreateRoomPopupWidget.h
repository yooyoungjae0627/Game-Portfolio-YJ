#pragma once

#include "Blueprint/UserWidget.h"
#include "MSCreateRoomPopupWidget.generated.h"

class UButton;
class UEditableText;
class UTextBlock;

/**
 * UMSCreateRoomPopupWidget
 *
 * 역할:
 * - "방 제목 / 최대 인원" 입력을 받는 팝업 UI
 * - Confirm/Cancel 클릭 이벤트를 LobbyWidget에게 델리게이트로 전달
 *
 * 주의:
 * - 서버 RPC 직접 호출 ❌ (UI는 입력만)
 * - 최종 요청은 LobbyWidget -> PlayerController(Server_CreateRoom)로 전달
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMSCreateRoomPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Public API (Delegates)
	// ---------------------------

	DECLARE_DELEGATE_TwoParams(FOnConfirmCreateRoom, const FString& /*RoomTitle*/, int32 /*MaxPlayers*/);
	DECLARE_DELEGATE(FOnCancelCreateRoom);

	void BindOnConfirm(FOnConfirmCreateRoom InOnConfirm);
	void BindOnCancel(FOnCancelCreateRoom InOnCancel);

	/** 팝업 초기 값 세팅(선택) */
	void InitDefaultValues(const FString& InDefaultTitle, int32 InDefaultMaxPlayers);

protected:
	// ---------------------------
	// Engine
	// ---------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// ---------------------------
	// Internal
	// ---------------------------

	UFUNCTION()
	void OnClicked_Confirm();

	UFUNCTION()
	void OnClicked_Cancel();

	bool TryParseMaxPlayers(int32& OutMaxPlayers) const;
	FString GetRoomTitleText() const;

	void ApplyDefaultValuesIfNeeded();

private:
	// ---------------------------
	// Widgets (BindWidget)
	// ---------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> EditableText_RoomTitle = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> EditableText_MaxPlayers = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Error = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Confirm = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Cancel = nullptr;

private:
	// ---------------------------
	// Delegates
	// ---------------------------

	FOnConfirmCreateRoom OnConfirmCreateRoom;
	FOnCancelCreateRoom OnCancelCreateRoom;

private:
	// ---------------------------
	// Defaults
	// ---------------------------

	FString DefaultTitle = TEXT("");
	int32 DefaultMaxPlayers;
	bool bDefaultsApplied = false;
};
