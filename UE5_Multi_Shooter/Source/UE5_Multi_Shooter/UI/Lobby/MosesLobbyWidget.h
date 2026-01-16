// ============================================================================
// MosesLobbyWidget.h
// ============================================================================

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
class UTextBlock;
class UEditableTextBox;
class UUserWidget;

class AMosesLobbyGameState;
class AMosesPlayerState;
class AMosesPlayerController;
class UMosesLobbyLocalPlayerSubsystem;
class UMSCreateRoomPopupWidget;

/**
 * UMosesLobbyWidget
 *
 * 책임(요약)
 * - 로비 UI의 “표시 + 입력” 담당 (버튼/리스트/채팅 입력)
 * - 서버 권한 로직은 절대 직접 처리하지 않고, PlayerController 서버 RPC로 위임한다.
 *
 * 단일 진실(Single Source of Truth)
 * - 룸/멤버/Ready/채팅 히스토리: AMosesLobbyGameState (복제)
 * - 내 상태(방 소속/호스트/로그인/Ready 등): AMosesPlayerState (복제)
 * - UI 갱신 트리거: UMosesLobbyLocalPlayerSubsystem 이벤트 (로컬 전용 브릿지)
 *
 * 안전장치
 * - BindWidgetOptional은 BP에서 누락될 수 있으므로 항상 nullptr 체크 후 사용한다.
 * - NativeConstruct/NativeDestruct에서 Delegate/Timer를 반드시 해제해 유령 바인딩을 방지한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/*====================================================
	= Engine Lifecycle
	====================================================*/
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	/*====================================================
	= Initialization / Binding
	====================================================*/
	void CacheLocalSubsystem();
	void BindLobbyButtons();
	void BindListViewEvents();
	void BindSubsystemEvents();
	void UnbindSubsystemEvents();

protected:
	/*====================================================
	= Unified UI Refresh
	====================================================*/
	void RefreshAll_UI();

protected:
	/*====================================================
	= UI Refresh Internals
	====================================================*/
	void RefreshRoomListFromGameState();
	void RefreshPanelsByPlayerState();
	void RefreshRightPanelControlsByRole();

protected:
	/*====================================================
	= Subsystem → UI Entry Points
	====================================================*/
	void HandleRoomStateChanged_UI();
	void HandlePlayerStateChanged_UI();

	// [DIALOGUE/VOICE DISABLED]
	// void HandleRulesViewModeChanged_UI(bool bEnable);

protected:
	/*====================================================
	= Create Room Popup Flow
	====================================================*/
	void OpenCreateRoomPopup();
	void CloseCreateRoomPopup();

	void HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers);
	void HandleCreateRoomPopupCancel();

	void CenterPopupWidget(UUserWidget* PopupWidget) const;

protected:
	/*====================================================
	= Start Game Policy
	====================================================*/
	bool CanStartGame_UIOnly() const;
	void UpdateStartButton();

protected:
	/*====================================================
	= UI Event Handlers
	====================================================*/
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
	void OnClicked_SendChat();

	void OnRoomItemClicked(UObject* ClickedItem);

protected:
	/*====================================================
	= Chat Helpers
	====================================================*/
	FString GetChatInput() const;
	void RebuildChat(const AMosesLobbyGameState* GS);

protected:
	/*====================================================
	= General Helpers
	====================================================*/
	AMosesPlayerController* GetMosesPC() const;
	AMosesPlayerState* GetMosesPS() const;
	UMosesLobbyLocalPlayerSubsystem* GetLobbySubsys() const;
	bool IsLocalHost() const;

protected:
	/*====================================================
	= Pending Enter Room (UI Only)
	====================================================*/
	void BeginPendingEnterRoom_UIOnly();
	void EndPendingEnterRoom_UIOnly();

	UFUNCTION()
	void OnPendingEnterRoomTimeout_UIOnly();

protected:
	/*====================================================
	= Join Room Result (Subsystem → UI)
	====================================================*/
	void HandleJoinRoomResult_UI(EMosesRoomJoinResult Result, const FGuid& RoomId);
	FString JoinFailReasonToText(EMosesRoomJoinResult Result) const;

public:
	/*====================================================
	= Public API (Subsystem → Widget)
	====================================================*/
	void RefreshFromState(const AMosesLobbyGameState* InMosesLobbyGameState, const AMosesPlayerState* InMosesPlayerState);

private:
	/*====================================================
	= Widgets (BindWidgetOptional)
	====================================================*/
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
	TObjectPtr<UVerticalBox> CharacterSelectedButtonsBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NickNameText = nullptr;

	// ---------------------------
	// Debug Command Input (중복 선언이 있었음: BP 호환 위해 둘 다 유지)
	// ---------------------------

	// (Legacy) - BP에서 이 이름으로 바인딩 중일 수 있음
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditableTextBox_Command = nullptr;

	// (Legacy) - BP에서 이 이름으로 바인딩 중일 수 있음
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SubmitCommand = nullptr;

	// (Current) - 프로젝트 표준 네이밍(권장)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ET_CommandInput = nullptr;

	// (Current) - 프로젝트 표준 네이밍(권장)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_SubmitCommand = nullptr;

	// ---------------------------
	// Chat
	// ---------------------------
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ChatListView = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ChatEditableTextBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_SendChat = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_MyRoom = nullptr;

private:
	/*====================================================
	= Cached Subsystem
	====================================================*/
	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyLocalPlayerSubsystem> LobbyLPS = nullptr;

private:
	/*====================================================
	= Create Room Popup Assets
	====================================================*/
	UPROPERTY(EditDefaultsOnly, Category = "UI|Popup")
	TSubclassOf<UMSCreateRoomPopupWidget> CreateRoomPopupClass;

	UPROPERTY(Transient)
	TObjectPtr<UMSCreateRoomPopupWidget> CreateRoomPopup = nullptr;

private:
	/*====================================================
	= Pending State (UI Only)
	====================================================*/
	UPROPERTY(Transient)
	bool bPendingEnterRoom_UIOnly = false;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	float PendingEnterRoomTimeoutSeconds = 2.0f;

	FTimerHandle PendingEnterRoomTimerHandle;

private:
	/*====================================================
	= Join deferred until login
	====================================================*/
	FGuid PendingJoinRoomId;
	bool bPendingJoinAfterLogin = false;
};
