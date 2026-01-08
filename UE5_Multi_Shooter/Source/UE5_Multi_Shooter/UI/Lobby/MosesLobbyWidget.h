// MosesLobbyWidget.h
#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

enum class EMosesRoomJoinResult : uint8;

// ✅ Dialogue net types forward
struct FDialogueNetState;
enum class EDialogueFlowState : uint8;
enum class EDialogueSubState : uint8;

// ---------------------------
// Forward Decls
// ---------------------------
class UOverlay;
class UButton;
class UCheckBox;
class UListView;
class UVerticalBox;
class UHorizontalBox;
class USizeBox;

class UWidget;
class UTextBlock;

// ✅ DataAsset
class UMosesDialogueLineDataAsset;

class UMosesLobbyLocalPlayerSubsystem;
class AMosesPlayerController;
class AMosesPlayerState;
class UMSCreateRoomPopupWidget;

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
	void HandleRulesViewModeChanged_UI(bool bEnable);

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
	void OnClicked_GameRules();

	UFUNCTION()
	void OnClicked_ExitDialogue();

	void OnRoomItemClicked(UObject* ClickedItem);

private:
	void RefreshGameRulesButtonVisibility();

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

private:
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

	// ✅ Bubble 컨테이너(너는 SizeBox 안에 BubbleRoot/TB를 직접 넣는다고 했음)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> DialogueBubbleWidget = nullptr;

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
	= RulesView state
	====================================================*/
	bool bRulesViewEnabled = false;

private:
	/*====================================================
	= Dialogue Bubble internals (✅ NEW - LobbyWidget 내부 트리 기준)
	====================================================*/
	void CacheDialogueBubble_InternalRefs();
	void HandleDialogueStateChanged_UI(const FDialogueNetState& NewState);
	void ApplyDialogueState_ToBubbleUI(const FDialogueNetState& NewState);

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

private:
	// ✅ 자막 라인 데이터(클라에서도 로드 가능한 DataAsset)
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	TObjectPtr<UMosesDialogueLineDataAsset> DialogueLineData = nullptr;

private:
	// ✅ LobbyWidget 내부에 직접 배치한 위젯들을 캐시
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CachedBubbleRoot = nullptr;      // 이름: "BubbleRoot"

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CachedTB_DialogueText = nullptr; // 이름: "TB_DialogueText"

private:
	// ✅ 중복 재생/깜빡임 방지 캐시
	UPROPERTY(Transient)
	int32 CachedDialogueSeq = 0;

	UPROPERTY(Transient)
	int32 CachedLineIndex = INDEX_NONE;

	UPROPERTY(Transient)
	EDialogueFlowState CachedFlowState = (EDialogueFlowState)0;

private:
	// ✅ Fade 상태
	FTimerHandle BubbleFadeTimerHandle;
	float BubbleFadeDurationSeconds = 0.25f;
	float BubbleFadeElapsed = 0.f;
	float BubbleFadeFrom = 0.f;
	float BubbleFadeTo = 1.f;
	bool bBubbleCollapseAfterFade = false;

	// GameRules button visibility log cache (spam guard)
	mutable int32 CachedGameRulesLogHash = INDEX_NONE;
};
