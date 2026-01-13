#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

struct FDialogueNetState;

enum class EMosesRoomJoinResult : uint8;
enum class EDialogueFlowState : uint8;
enum class EDialogueSubState : uint8;

class UOverlay;
class UButton;
class UCheckBox;
class UListView;
class UVerticalBox;
class UHorizontalBox;
class USizeBox;
class UProgressBar;
class UWidget;
class UTextBlock;
class UEditableTextBox;
class AMosesLobbyGameState;
class AMosesPlayerState;
class UMosesDialogueLineDataAsset;
class UMosesLobbyLocalPlayerSubsystem;
class AMosesPlayerController;
class UMSCreateRoomPopupWidget;

static const TCHAR* NetModeToString(ENetMode Mode)
{
	switch (Mode)
	{
	case NM_Standalone:      return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer:    return TEXT("ListenServer");
	case NM_Client:          return TEXT("Client");
	default:                 return TEXT("Unknown");
	}
}

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

	/*====================================================
	= Initialization / Binding
	====================================================*/
	void CacheLocalSubsystem();
	void BindLobbyButtons();
	void BindListViewEvents();
	void BindSubsystemEvents();
	void UnbindSubsystemEvents();

	/*====================================================
	= Unified UI Refresh
	====================================================*/
	void RefreshAll_UI();

	/*====================================================
	= UI Refresh Internals
	====================================================*/
	void RefreshRoomListFromGameState();
	void RefreshPanelsByPlayerState();
	void RefreshRightPanelControlsByRole();

	/*====================================================
	= Subsystem → UI Entry Points
	====================================================*/
	void HandleRoomStateChanged_UI();
	void HandlePlayerStateChanged_UI();
	void HandleRulesViewModeChanged_UI(bool bEnable);

	/*====================================================
	= Create Room Popup Flow
	====================================================*/
	void OpenCreateRoomPopup();
	void CloseCreateRoomPopup();

	void HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers);
	void HandleCreateRoomPopupCancel();

	void CenterPopupWidget(UUserWidget* PopupWidget) const;

	/*====================================================
	= Start Game Policy
	====================================================*/
	bool CanStartGame_UIOnly() const;
	void UpdateStartButton();

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
	void OnClicked_GameRules();

	UFUNCTION()
	void OnClicked_ExitDialogue();

	UFUNCTION() 
	void OnClicked_SendChat();


	void OnRoomItemClicked(UObject* ClickedItem);

	void RefreshGameRulesButtonVisibility();

	FString GetChatInput() const;
	void RebuildChat(const AMosesLobbyGameState* GS);

protected:
	/*====================================================
	= Helpers
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
	= Public UI Mode Control
	====================================================*/
	void SetRulesViewMode(bool bEnable);


	void RefreshFromState(const AMosesLobbyGameState* InMosesLobbyGameState, const AMosesPlayerState* InMosesPlayerState);

private:
	/*====================================================
	= Dialogue Bubble internals (✅ NEW - LobbyWidget 내부 트리 기준)
	====================================================*/
	void CacheDialogueBubble_InternalRefs();
	void HandleDialogueStateChanged_UI(const FDialogueNetState& NewState);
	
	// ✅ 전광판 NetState 적용 (텍스트 업데이트 누락 방지 포함)
	void ApplyDialogueState_ToBubbleUI(const FDialogueNetState& NewState);

	// ✅ 메타휴먼(시점/카메라) 모드 진입 시: "자막 텍스트"만 강제 Collapsed
	void OnEnterMetaHumanView_SubtitleOnly();

	// ✅ 메타휴먼(시점/카메라) 모드 종료 후 복귀 시:
	// - 자막 텍스트 Visible
	// - 최신 상태를 1회 강제 재적용(숨김/복귀에서 텍스트 안 바뀌는 문제 종결)
	void OnExitMetaHumanView_SubtitleOnly_Reapply(const FDialogueNetState& LatestState);

	void ShowDialogueBubble_UI(bool bPlayFadeIn);
	void HideDialogueBubble_UI(bool bPlayFadeOut);
	void SetDialogueBubbleText_UI(const FText& Text);

	bool ShouldShowBubbleInCurrentMode(const FDialogueNetState& NetState) const;
	FText GetSubtitleTextFromNetState(const FDialogueNetState& NetState) const;

	// ✅ Fade = BP 애니메이션 안 쓰고, 타이머로 RenderOpacity 보간
	void StartBubbleFade(float FromOpacity, float ToOpacity, bool bCollapseAfterFade);
	void TickBubbleFade();
	void StopBubbleFade();
	void SetBubbleOpacity(float Opacity);


	void SetSubtitleVisibility(bool bVisible);

	UFUNCTION()
	void OnClicked_MicToggle();

	UFUNCTION()
	void OnClicked_SubmitCommand();

	void BindVoiceSubsystemEvents();
	void UnbindVoiceSubsystemEvents();
	void HandleVoiceLevelChanged(float Level01);

private:
	// ✅ 자막 라인 데이터(클라에서도 로드 가능한 DataAsset)
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	TObjectPtr<UMosesDialogueLineDataAsset> DialogueLineData = nullptr;

	// ✅ LobbyWidget 내부에 직접 배치한 위젯들을 캐시
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CachedBubbleRoot = nullptr;      // 이름: "BubbleRoot"

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CachedTB_DialogueText = nullptr; // 이름: "TB_DialogueText"

	// ✅ 중복 재생/깜빡임 방지 캐시
	UPROPERTY(Transient)
	int32 CachedDialogueSeq = 0;

	UPROPERTY(Transient)
	int32 CachedLineIndex = INDEX_NONE;

	UPROPERTY(Transient)
	EDialogueFlowState CachedFlowState;

	/*====================================================
	= Widgets (BindWidget)
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
	TObjectPtr<UButton> Btn_GameRules = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_ExitDialogue = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> CharacterSelectedButtonsBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> DialogueBubbleWidget = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NickNameText = nullptr;


	UPROPERTY(meta = (BindWidgetOptional)) 
	UButton* Button_MicToggle = nullptr;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	UProgressBar* ProgressBar_MicLevel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) 
	UEditableTextBox* EditableTextBox_Command = nullptr;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	UButton* Button_SubmitCommand = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) 
	UTextBlock* Text_DialogueBubble = nullptr;   // TB_DialogueText 같은 것
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	UTextBlock* Text_DialogueStatus = nullptr;  // "Paused" 등 표시용(옵션)


	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UListView> ChatListView = nullptr;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UEditableTextBox> ChatEditableTextBox = nullptr;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> BTN_SendChat = nullptr;

	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UTextBlock> TXT_MyRoom = nullptr;


	// ---------------------------
	// Mic UI
	// ---------------------------
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_MicToggle = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_MicLevel = nullptr;

	// ---------------------------
	// Debug Command Input
	// ---------------------------
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ET_CommandInput = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_SubmitCommand = nullptr;

	/*====================================================
	= Cached Subsystem
	====================================================*/
	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyLocalPlayerSubsystem> LobbyLPS = nullptr;

	/*====================================================
	= Create Room Popup Assets
	====================================================*/
	UPROPERTY(EditDefaultsOnly, Category = "UI|Popup")
	TSubclassOf<UMSCreateRoomPopupWidget> CreateRoomPopupClass;

	UPROPERTY(Transient)
	TObjectPtr<UMSCreateRoomPopupWidget> CreateRoomPopup = nullptr;

	/*====================================================
	= Pending State (UI Only)
	====================================================*/
	UPROPERTY(Transient)
	bool bPendingEnterRoom_UIOnly = false;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	float PendingEnterRoomTimeoutSeconds = 2.0f;

	FTimerHandle PendingEnterRoomTimerHandle;

	/*====================================================
	= RulesView state
	====================================================*/
	bool bRulesViewEnabled = false;

	// ✅ Fade 상태
	FTimerHandle BubbleFadeTimerHandle;
	float BubbleFadeDurationSeconds = 0.25f;
	float BubbleFadeElapsed = 0.f;
	float BubbleFadeFrom = 0.f;
	float BubbleFadeTo = 1.f;
	bool bBubbleCollapseAfterFade = false;

	// GameRules button visibility log cache (spam guard)
	mutable int32 CachedGameRulesLogHash = INDEX_NONE;

	// ✅ "자막 숨김 상태에서 업데이트가 들어오면" 여기 저장해뒀다가 복귀 시 강제 적용
	FText PendingSubtitleText;

	// ✅ 메타휴먼 모드에서 자막을 강제로 내렸는지 추적 (복귀 시만 올리려고)
	bool bSubtitleForcedCollapsedByMetaHuman = false;

};
