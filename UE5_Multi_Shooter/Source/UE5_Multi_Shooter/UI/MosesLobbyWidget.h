#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;

/**
 * UMosesLobbyWidget
 *
 * - WBP_Lobby의 로직 담당(C++)
 * - 디자이너는 WBP에서 유지
 * - 버튼 클릭/체크 이벤트 -> PlayerController RPC 호출
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	/** Owning Player를 MosesPlayerController로 캐스팅해서 반환 */
	class AMosesPlayerController* GetMosesPC() const;

	// ---- UI Widgets (WBP에서 "Is Variable" 체크 + 이름 일치 필요) ----
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CreateRoom;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_JoinRoom;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_LeaveRoom;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_StartGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> CheckBox_Ready;

	// Join RoomId 입력용
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> TextBox_RoomId;

	// ---- Event handlers ----
	UFUNCTION()
	void OnClicked_CreateRoom();

	UFUNCTION()
	void OnClicked_JoinRoom();

	UFUNCTION()
	void OnClicked_LeaveRoom();

	UFUNCTION()
	void OnClicked_StartGame();

	UFUNCTION()
	void OnReadyChanged(bool bIsChecked);

private:
	/** 임시: CreateRoom MaxPlayers */
	UPROPERTY(EditAnywhere, Category = "Lobby|Debug")
	int32 DebugMaxPlayers = 4;
};
