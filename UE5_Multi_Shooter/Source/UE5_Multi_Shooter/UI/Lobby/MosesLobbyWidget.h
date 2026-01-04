// MosesLobbyWidget.h
#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

class UButton;
class UCheckBox;
class UListView;
class UVerticalBox;

class UMosesLobbyLocalPlayerSubsystem;
class AMosesPlayerController;
class UMSCreateRoomPopupWidget;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ---------------------------
	// Engine
	// ---------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// ---------------------------
	// Setup / Bind
	// ---------------------------
	void CacheLocalSubsystem();
	void BindLobbyButtons();
	void BindCharacterButtons();
	void BindListViewEvents();
	void BindSubsystemEvents();
	void UnbindSubsystemEvents();

	// ---------------------------
	// Lobby UI refresh internals
	// ---------------------------
	void RefreshRoomListFromGameState();
	void RefreshPanelsByPlayerState();
	void RefreshRightPanelControlsByRole();

	// ---------------------------
	// UI Refresh Entry (Subsystem에서 호출)
	// ---------------------------
	void HandleRoomStateChanged_UI();
	void HandlePlayerStateChanged_UI();

	// ---------------------------
	// Create Room Popup
	// ---------------------------
	void OpenCreateRoomPopup();
	void CloseCreateRoomPopup();

	void HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers);
	void HandleCreateRoomPopupCancel();

	void CenterPopupWidget(UUserWidget* PopupWidget) const;

	// ---------------------------
	// Start button policy
	// ---------------------------
	bool CanStartGame_UIOnly() const;
	void UpdateStartButton();

	// ---------------------------
	// UI Event handlers
	// ---------------------------
	UFUNCTION()
	void OnClicked_CreateRoom();

	UFUNCTION()
	void OnClicked_LeaveRoom();

	UFUNCTION()
	void OnClicked_StartGame();

	UFUNCTION()
	void OnReadyChanged(bool bIsChecked);

	UFUNCTION()
	void OnClicked_CharPrev();

	UFUNCTION()
	void OnClicked_CharNext();

	void OnRoomItemClicked(UObject* ClickedItem);

	// ---------------------------
	// Helpers
	// ---------------------------
	AMosesPlayerController* GetMosesPC() const;

	// ---------------------------
	// Pending Enter Room (UI Only)
	// ---------------------------
	void BeginPendingEnterRoom_UIOnly();
	void EndPendingEnterRoom_UIOnly();

	UFUNCTION()
	void OnPendingEnterRoomTimeout_UIOnly();

private:
	// ---------------------------
	// Widgets (BindWidget)
	// ---------------------------
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CreateRoom = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_LeaveRoom = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_StartGame = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> CheckBox_Ready = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> RoomListView = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> LeftPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RightPanel = nullptr;

	// 개발자 주석:
	// - WBP에서 이름을 Btn_CharPrev / Btn_CharNext 로 맞춰라.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_CharPrev = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_CharNext = nullptr;

private:
	// ---------------------------
	// Cached subsystem
	// ---------------------------
	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyLocalPlayerSubsystem> LobbyLPS = nullptr;

private:
	// ---------------------------
	// Create Room Popup assets
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "UI|Popup")
	TSubclassOf<UMSCreateRoomPopupWidget> CreateRoomPopupClass;

	UPROPERTY(Transient)
	TObjectPtr<UMSCreateRoomPopupWidget> CreateRoomPopup = nullptr;

private:
	// ---------------------------
	// Debug / Defaults
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Debug")
	int32 DebugMaxPlayers = 4;

private:
	// ---------------------------
	// Pending state (UI-only)
	// ---------------------------
	UPROPERTY(Transient)
	bool bPendingEnterRoom_UIOnly = false;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	float PendingEnterRoomTimeoutSeconds = 2.0f;

	FTimerHandle PendingEnterRoomTimerHandle;
};
