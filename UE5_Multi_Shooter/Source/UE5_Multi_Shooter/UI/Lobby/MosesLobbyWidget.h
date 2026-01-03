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

/**
 * UMosesLobbyWidget
 *
 * 역할(클라 UI):
 * - 버튼/체크 이벤트 바인딩
 * - 입력값 1차 검증 후 PlayerController 서버 RPC 호출
 * - 복제 데이터(PS/GS)에 따라 UI 상태 갱신
 *
 * 원칙:
 * - UI는 "표시/입력"만 한다.
 * - Start 가능 최종 판단/Travel은 서버(GameMode/GameState).
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Rep Notify Entry (Subsystem에서 호출)
	// ---------------------------

	/** RoomList/Ready/정원 등 "룸 관련 복제 상태" 변경 시 UI 갱신 진입점 */
	void HandleRoomStateChanged_UI();

	/** Host/Ready/RoomId 등 "내 PlayerState 관련 복제 상태" 변경 시 UI 갱신 진입점 */
	void HandlePlayerStateChanged_UI();

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
	void BindListViewEvents();

	// ---------------------------
	// Lobby UI refresh internals
	// ---------------------------
	void RefreshRoomListFromGameState();
	void RefreshPanelsByPlayerState();
	void RefreshRightPanelControlsByRole();

	// ---------------------------
	// Create Room Popup
	// ---------------------------
	void OpenCreateRoomPopup();
	void CloseCreateRoomPopup();

	void HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers);
	void HandleCreateRoomPopupCancel();

	void CenterPopupWidget(UUserWidget* PopupWidget) const;

	// ---------------------------
	// Start button policy (내부 전용)
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

	void OnRoomItemClicked(UObject* ClickedItem);

	// ---------------------------
	// Helpers
	// ---------------------------
	AMosesPlayerController* GetMosesPC() const;

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
};
