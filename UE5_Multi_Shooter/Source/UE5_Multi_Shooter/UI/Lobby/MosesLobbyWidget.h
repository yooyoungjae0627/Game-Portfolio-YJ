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

class UUserWidget;
class UTextBlock;
class UWidgetAnimation;

// ✅ DataAsset
class UMosesDialogueLineDataAsset;

class UMosesDialogueBubbleWidget;
class UMosesLobbyLocalPlayerSubsystem;
class AMosesPlayerController;
class AMosesPlayerState;
class UMSCreateRoomPopupWidget;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	void CacheLocalSubsystem();
	void BindLobbyButtons();
	void BindListViewEvents();
	void BindSubsystemEvents();
	void UnbindSubsystemEvents();

protected:
	void RefreshAll_UI();

protected:
	void RefreshRoomListFromGameState();
	void RefreshPanelsByPlayerState();
	void RefreshRightPanelControlsByRole();

protected:
	void HandleRoomStateChanged_UI();
	void HandlePlayerStateChanged_UI();
	void HandleRulesViewModeChanged_UI(bool bEnable);

protected:
	void OpenCreateRoomPopup();
	void CloseCreateRoomPopup();

	void HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers);
	void HandleCreateRoomPopupCancel();

	void CenterPopupWidget(UUserWidget* PopupWidget) const;

protected:
	bool CanStartGame_UIOnly() const;
	void UpdateStartButton();

protected:
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

protected:
	AMosesPlayerController* GetMosesPC() const;
	AMosesPlayerState* GetMosesPS() const;
	UMosesLobbyLocalPlayerSubsystem* GetLobbySubsys() const;
	bool IsLocalHost() const;

protected:
	void BeginPendingEnterRoom_UIOnly();
	void EndPendingEnterRoom_UIOnly();

	UFUNCTION()
	void OnPendingEnterRoomTimeout_UIOnly();

protected:
	void HandleJoinRoomResult_UI(EMosesRoomJoinResult Result, const FGuid& RoomId);
	FString JoinFailReasonToText(EMosesRoomJoinResult Result) const;

public:
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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> DialogueBubbleWidget = nullptr;

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

private:
	/*====================================================
	= Dialogue Bubble internals (✅ NEW)
	====================================================*/
	void CacheDialogueBubble_InternalRefs();
	void HandleDialogueStateChanged_UI(const FDialogueNetState& NewState);
	void ApplyDialogueState_ToBubbleUI(const FDialogueNetState& NewState);

	void ShowDialogueBubble_UI(bool bPlayFadeIn);
	void HideDialogueBubble_UI(bool bPlayFadeOut);
	void SetDialogueBubbleText_UI(const FText& Text);

	void PlayDialogueFadeIn_UI();
	void PlayDialogueFadeOut_UI();

	FText GetSubtitleTextFromNetState(const FDialogueNetState& NetState) const;
	bool ShouldShowBubbleInCurrentMode(const FDialogueNetState& NetState) const;

	// ✅ 자막 라인 데이터(클라에서도 로드 가능한 DataAsset)
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	TObjectPtr<UMosesDialogueLineDataAsset> DialogueLineData = nullptr;

	// Bubble 내부 위젯(= SizeBox Content가 UserWidget이어야 함)
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CachedBubbleUserWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CachedTB_DialogueText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> CachedAnim_FadeIn = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> CachedAnim_FadeOut = nullptr;

	// 중복 재생/깜빡임 방지
	UPROPERTY(Transient)
	int32 CachedDialogueSeq = 0;

	UPROPERTY(Transient)
	int32 CachedLineIndex = INDEX_NONE;

	UPROPERTY(Transient)
	EDialogueFlowState CachedFlowState = (EDialogueFlowState)0;
};
