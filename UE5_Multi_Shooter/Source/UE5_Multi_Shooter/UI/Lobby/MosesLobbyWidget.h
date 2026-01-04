// MosesLobbyWidget.h
#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

enum class EMosesRoomJoinResult : uint8;

class UButton;
class UCheckBox;
class UListView;
class UVerticalBox;
class UHorizontalBox;

class UMosesLobbyLocalPlayerSubsystem;
class AMosesPlayerController;
class AMosesPlayerState;
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

	// ---------------------------
	// Setup / Bind
	// ---------------------------
	void CacheLocalSubsystem();
	void BindLobbyButtons();
	void BindListViewEvents();
	void BindSubsystemEvents();
	void UnbindSubsystemEvents();

	// ---------------------------
	// Unified Refresh
	// ---------------------------
	void RefreshAll_UI();

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
	AMosesPlayerState* GetMosesPS() const;

	// ---------------------------
	// Pending Enter Room (UI Only)
	// ---------------------------
	void BeginPendingEnterRoom_UIOnly();
	void EndPendingEnterRoom_UIOnly();

	UFUNCTION()
	void OnPendingEnterRoomTimeout_UIOnly();

	// ---------------------------
	// JoinRoom Result (from Subsystem)
	// ---------------------------
	void HandleJoinRoomResult_UI(EMosesRoomJoinResult Result, const FGuid& RoomId);
	FString JoinFailReasonToText(EMosesRoomJoinResult Result) const;

	// ---------------------------
	// UI Update
	// ---------------------------

	/** 현재 로컬 플레이어(내가 보는 화면)의 상태로 UI를 갱신한다 */
	void RefreshHostClientPanels();

	/** 로컬 PC 가져오기 */
	AMosesPlayerController* GetMosesPC_Local() const;

	/** 로컬 PS 가져오기 */
	AMosesPlayerState* GetMosesPS_Local() const;

	/** 방 입장 상태인지(Host/Client 공통) */
	bool IsLocalInRoom() const;

	/** Host인지 */
	bool IsLocalHost() const;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ClientPanel = nullptr;

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
	// Pending state (UI-only)
	// ---------------------------
	UPROPERTY(Transient)
	bool bPendingEnterRoom_UIOnly = false;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	float PendingEnterRoomTimeoutSeconds = 2.0f;

	FTimerHandle PendingEnterRoomTimerHandle;
};
