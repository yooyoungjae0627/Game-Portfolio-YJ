// MosesLobbyWidget.h
#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

enum class EMosesRoomJoinResult : uint8;

class UOverlay;
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
	void OnClicked_CharNext();

	UFUNCTION()
	void OnClicked_GameRules();

	UFUNCTION()
	void OnClicked_ExitDialogue();


	void OnRoomItemClicked(UObject* ClickedItem);

	// ---------------------------
	// Helpers
	// ---------------------------
	AMosesPlayerController* GetMosesPC() const;
	AMosesPlayerState* GetMosesPS() const;
	UMosesLobbyLocalPlayerSubsystem* GetLobbySubsys() const;

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


	/** Host인지 */
	bool IsLocalHost() const;

public:
	/** 
	 *  RulesView(게임 진행 방법 보기) 진입/복귀 시
	 *  Widget 내부 패널/버튼 가시성만 책임진다.
	 *  (카메라 전환/상태 판단은 Subsystem)
	 */
	void SetRulesViewMode(bool bEnable);


private:
	// ---------------------------
	// Widgets (BindWidget)
	// ---------------------------
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CreateRoom = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_LeaveRoom = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_StartGame = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> CheckBox_Ready = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> RoomListView = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> LeftPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RightPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ClientPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_SelectedChracter = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> RoomListViewOverlay = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_GameRules = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_ExitDialogue = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> CharacterSelectedButtonsBox = nullptr;

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

	// UI 꼬임 방지용 로컬 캐시
	bool bRulesViewEnabled = false;
};
