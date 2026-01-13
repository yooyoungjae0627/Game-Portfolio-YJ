// MosesLobbyWidget.cpp

#include "MosesLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ListView.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/EditableTextBox.h"

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/UI/Lobby/MSCreateRoomPopupWidget.h"
#include "UE5_Multi_Shooter/UI/Lobby/MSLobbyRoomItemWidget.h"

#include "UE5_Multi_Shooter/Dialogue/MosesLobbyVoiceSubsystem.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueLineDataAsset.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatTypes.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatRowItem.h"


// =====================================================
// Engine Lifecycle
// =====================================================

void UMosesLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] NativeConstruct OwningPlayer=%s OwningLP=%s World=%s"),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningLocalPlayer()),
		*GetNameSafe(GetWorld()));

	// ✅ Bubble은 Construct에서 강제로 숨기지 않는다.
	// - '항상 보임' 정책에서는 대화 NetState가 오면 바로 표시돼야 함
	// - 초기에는 "보여줄 게 없으면 Apply에서 Hide"가 처리한다.
	if (DialogueBubbleWidget)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyUI] DialogueBubbleWidget OK = %s"), *GetNameSafe(DialogueBubbleWidget));

		// 안전 기본값: 일단 Hidden으로만 두고(레이아웃 유지), 상태 반영에서 최종 결정
		DialogueBubbleWidget->SetVisibility(ESlateVisibility::Hidden);
		SetBubbleOpacity(1.f);
	}
	else
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[LobbyUI] DialogueBubbleWidget is NULL. (BindWidgetOptional failed)"));
	}

	if (RightPanel)
	{
		RightPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (LeftPanel)
	{
		LeftPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (RoomListViewOverlay)
	{
		RoomListViewOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	CacheLocalSubsystem();
	BindLobbyButtons();
	BindListViewEvents();
	BindSubsystemEvents();

	CacheDialogueBubble_InternalRefs();

	HideDialogueBubble_UI(true);

	// RulesView OFF로 시작
	SetRulesViewMode(false);

	RefreshAll_UI();

	// ✅ 대화 상태 즉시 복구 (late join / 위젯 재생성)
	if (LobbyLPS)
	{
		HandleDialogueStateChanged_UI(LobbyLPS->GetLastDialogueNetState());
	}
}

void UMosesLobbyWidget::NativeDestruct()
{
	UnbindSubsystemEvents();

	StopBubbleFade();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingEnterRoomTimerHandle);
	}

	if (Button_CreateRoom)
	{
		Button_CreateRoom->OnClicked.RemoveAll(this);
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->OnClicked.RemoveAll(this);
	}

	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.RemoveAll(this);
	}

	if (Btn_GameRules)
	{
		Btn_GameRules->OnClicked.RemoveAll(this);
	}

	if (Btn_ExitDialogue)
	{
		Btn_ExitDialogue->OnClicked.RemoveAll(this);
	}

	if (CheckBox_Ready)
	{
		CheckBox_Ready->OnCheckStateChanged.RemoveAll(this);
	}

	if (RoomListView)
	{
		RoomListView->OnItemClicked().RemoveAll(this);
	}

	if (Btn_SelectedChracter)
	{
		Btn_SelectedChracter->OnClicked.RemoveAll(this);
	}

	if (BTN_SendChat)
	{
		BTN_SendChat->OnClicked.RemoveAll(this);
	}

	LobbyLPS = nullptr;

	CloseCreateRoomPopup();

	Super::NativeDestruct();
}

// =====================================================
// Initialization / Binding
// =====================================================

void UMosesLobbyWidget::CacheLocalSubsystem()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	LobbyLPS = LP ? LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>() : nullptr;

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] CacheLocalSubsystem OwningLP=%s LobbyLPS=%s"),
		*GetNameSafe(LP),
		*GetNameSafe(LobbyLPS));
}

void UMosesLobbyWidget::BindLobbyButtons()
{
	if (Button_CreateRoom)
	{
		Button_CreateRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CreateRoom);
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_LeaveRoom);
	}

	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_StartGame);
	}

	if (CheckBox_Ready)
	{
		CheckBox_Ready->OnCheckStateChanged.AddDynamic(this, &UMosesLobbyWidget::OnReadyChanged);
	}

	if (Btn_SelectedChracter)
	{
		Btn_SelectedChracter->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CharNext);
	}

	if (Btn_GameRules)
	{
		Btn_GameRules->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_GameRules);
	}

	if (Btn_ExitDialogue)
	{
		Btn_ExitDialogue->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_ExitDialogue);
	}

	if (Btn_MicToggle)
	{
		Btn_MicToggle->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_MicToggle);
	}

	if (Btn_SubmitCommand)
	{
		Btn_SubmitCommand->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_SubmitCommand);
	}

	if (BTN_SendChat)
	{
		BTN_SendChat->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_SendChat);
	}
}

void UMosesLobbyWidget::BindListViewEvents()
{
	if (!RoomListView)
	{
		return;
	}

	RoomListView->OnItemClicked().AddUObject(this, &UMosesLobbyWidget::OnRoomItemClicked);
}

void UMosesLobbyWidget::BindSubsystemEvents()
{
	if (!LobbyLPS)
	{
		return;
	}

	LobbyLPS->OnLobbyPlayerStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyRoomStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyJoinRoomResult().RemoveAll(this);
	LobbyLPS->OnRulesViewModeChanged().RemoveAll(this);

	// ✅ DialogueNetState 이벤트 구독
	LobbyLPS->OnLobbyDialogueStateChanged().RemoveAll(this);

	LobbyLPS->OnLobbyPlayerStateChanged().AddUObject(this, &UMosesLobbyWidget::HandlePlayerStateChanged_UI);
	LobbyLPS->OnLobbyRoomStateChanged().AddUObject(this, &UMosesLobbyWidget::HandleRoomStateChanged_UI);
	LobbyLPS->OnLobbyJoinRoomResult().AddUObject(this, &UMosesLobbyWidget::HandleJoinRoomResult_UI);
	LobbyLPS->OnRulesViewModeChanged().AddUObject(this, &UMosesLobbyWidget::HandleRulesViewModeChanged_UI);

	// ✅ DialogueNetState 이벤트 바인딩
	LobbyLPS->OnLobbyDialogueStateChanged().AddUObject(this, &UMosesLobbyWidget::HandleDialogueStateChanged_UI);
}

void UMosesLobbyWidget::UnbindSubsystemEvents()
{
	if (!LobbyLPS)
	{
		return;
	}

	LobbyLPS->OnLobbyPlayerStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyRoomStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyJoinRoomResult().RemoveAll(this);
	LobbyLPS->OnRulesViewModeChanged().RemoveAll(this);

	// ✅ DialogueNetState 이벤트 해제
	LobbyLPS->OnLobbyDialogueStateChanged().RemoveAll(this);
}

// =====================================================
// Unified UI Refresh
// =====================================================

void UMosesLobbyWidget::RefreshAll_UI()
{
	RefreshPanelsByPlayerState();
	RefreshRightPanelControlsByRole();

	// 개발자 주석:
	// - 방 밖일 때만 방 리스트 갱신 (방 안에서 리스트 갱신하면 왼쪽 패널 깜빡이기 쉬움)
	const AMosesPlayerState* PS = GetMosesPS();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid()) || bPendingEnterRoom_UIOnly;
	if (!bInRoom)
	{
		RefreshRoomListFromGameState();
	}

	UpdateStartButton();
	RefreshGameRulesButtonVisibility();
}

// =====================================================
// UI Refresh Internals
// =====================================================

void UMosesLobbyWidget::RefreshRoomListFromGameState()
{
	if (!RoomListView)
	{
		return;
	}

	const AMosesLobbyGameState* LGS = GetWorld()
		? GetWorld()->GetGameState<AMosesLobbyGameState>()
		: nullptr;

	if (!LGS)
	{
		return;
	}

	// 1) 방이 하나라도 있는가?
	const bool bHasAnyRoom = (LGS->GetRooms().Num() > 0);

	// 2) 나는 방 밖인가? (Rep or Pending 기준)
	const AMosesPlayerState* PS = GetMosesPS();
	const bool bInRoomByRep = (PS && PS->GetRoomId().IsValid());
	const bool bInRoom_UIOnly = bInRoomByRep || bPendingEnterRoom_UIOnly;

	// ✅ Overlay는 "방이 존재" + "내가 방 밖"일 때만 보여준다.
	if (RoomListViewOverlay)
	{
		const bool bShouldShowOverlay = bHasAnyRoom && !bInRoom_UIOnly;

		RoomListViewOverlay->SetVisibility(
			bShouldShowOverlay ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
		);
	}

	// 방 밖일 때만 리스트 갱신 (너의 기존 정책 유지)
	RoomListView->ClearListItems();

	for (const FMosesLobbyRoomItem& Room : LGS->GetRooms())
	{
		UMSLobbyRoomListItemData* Item = NewObject<UMSLobbyRoomListItemData>(this);
		Item->Init(Room.RoomId, Room.RoomTitle, Room.MaxPlayers, Room.MemberPids.Num(), EMSLobbyRoomItemWidgetType::LeftPanel);
		RoomListView->AddItem(Item);
	}

	const APlayerController* PC = GetOwningPlayer();
	const int32 bAuth = (PC && PC->HasAuthority()) ? 1 : 0;

	const UWorld* World = GetWorld();

	UE_LOG(LogTemp, Warning, TEXT("[RoomUI] World=%s NetMode=%s PCAuth=%d Rooms=%d"),
		*GetNameSafe(World),
		World ? NetModeToString(World->GetNetMode()) : TEXT("None"),
		(PC && PC->HasAuthority()) ? 1 : 0,
		LGS ? LGS->GetRooms().Num() : -1);

}

void UMosesLobbyWidget::RefreshPanelsByPlayerState()
{
	const AMosesPlayerState* PS = GetMosesPS();

	// 개발자 주석:
	// - bPendingEnterRoom_UIOnly = "서버 승인 대기" 중 UI만 미리 방 안처럼 보이게 하는 플래그
	const bool bInRoomByRep = (PS && PS->GetRoomId().IsValid());
	const bool bInRoom_UIOnly = bInRoomByRep || bPendingEnterRoom_UIOnly;

	const ESlateVisibility NewLeft = bInRoom_UIOnly ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	const ESlateVisibility NewRight = bInRoom_UIOnly ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (LeftPanel && LeftPanel->GetVisibility() != NewLeft)
	{
		LeftPanel->SetVisibility(NewLeft);
	}

	if (RightPanel && RightPanel->GetVisibility() != NewRight)
	{
		RightPanel->SetVisibility(NewRight);
	}
}

void UMosesLobbyWidget::RefreshRightPanelControlsByRole()
{
	const AMosesPlayerState* PS = GetMosesPS();
	if (!PS)
	{
		return;
	}

	const bool bInRoom = PS->GetRoomId().IsValid() || bPendingEnterRoom_UIOnly;
	const bool bIsHost = PS->IsRoomHost();

	// 전환 중이면, Host/Client 구분 UI를 잠깐이라도 보여주지 않기
	const bool bAllowRoleSpecificUI = (bInRoom && !bPendingEnterRoom_UIOnly);

	if (Button_StartGame)
	{
		Button_StartGame->SetVisibility((bAllowRoleSpecificUI && bIsHost) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CheckBox_Ready)
	{
		const bool bShowReady = (bAllowRoleSpecificUI && !bIsHost);

		CheckBox_Ready->SetVisibility(bShowReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		CheckBox_Ready->SetIsEnabled(bShowReady);

		// ✅ 방 밖/전환중/호스트면 체크는 항상 false로
		if (!bShowReady)
		{
			CheckBox_Ready->SetIsChecked(false);
		}
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->SetVisibility(bAllowRoleSpecificUI ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_LeaveRoom->SetIsEnabled(bAllowRoleSpecificUI);
	}

	if (ClientPanel)
	{
		if (!bAllowRoleSpecificUI)
		{
			ClientPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ClientPanel->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

// =====================================================
// Subsystem → UI Entry Points
// =====================================================

void UMosesLobbyWidget::HandleRoomStateChanged_UI()
{
	RefreshAll_UI();
}

void UMosesLobbyWidget::HandlePlayerStateChanged_UI()
{
	const AMosesPlayerState* PS = GetMosesPS();

	if (bPendingJoinAfterLogin && PS && PS->IsLoggedIn() && PendingJoinRoomId.IsValid())
	{
		bPendingJoinAfterLogin = false;

		if (AMosesPlayerController* PC = GetMosesPC())
		{
			BeginPendingEnterRoom_UIOnly();
			PC->Server_JoinRoom(PendingJoinRoomId);
			UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] Auto-Join after login. Room=%s"), *PendingJoinRoomId.ToString());
		}
		return;
	}

	// 기존 로직 유지
	if (PS && PS->GetRoomId().IsValid() && bPendingEnterRoom_UIOnly)
	{
		EndPendingEnterRoom_UIOnly();
		return;
	}

	RefreshAll_UI();
}

void UMosesLobbyWidget::HandleRulesViewModeChanged_UI(bool bEnable)
{
	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyUI] HandleRulesViewModeChanged_UI bEnable=%d"), bEnable ? 1 : 0);
	SetRulesViewMode(bEnable);
}

// =====================================================
// Create Room Popup Flow
// =====================================================

void UMosesLobbyWidget::OpenCreateRoomPopup()
{
	if (!CreateRoomPopupClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] CreateRoomPopupClass NULL"));
		return;
	}

	if (!CreateRoomPopup)
	{
		CreateRoomPopup = CreateWidget<UMSCreateRoomPopupWidget>(GetOwningPlayer(), CreateRoomPopupClass);
		if (!CreateRoomPopup)
		{
			UE_LOG(LogTemp, Error, TEXT("[LobbyUI] CreateRoomPopup CreateWidget FAIL"));
			return;
		}

		CreateRoomPopup->BindOnConfirm(
			UMSCreateRoomPopupWidget::FOnConfirmCreateRoom::CreateUObject(
				this, &UMosesLobbyWidget::HandleCreateRoomPopupConfirm));

		CreateRoomPopup->BindOnCancel(
			UMSCreateRoomPopupWidget::FOnCancelCreateRoom::CreateUObject(
				this, &UMosesLobbyWidget::HandleCreateRoomPopupCancel));
	}

	if (!CreateRoomPopup->IsInViewport())
	{
		CreateRoomPopup->AddToViewport(50);
	}

	CenterPopupWidget(CreateRoomPopup);
}

void UMosesLobbyWidget::CloseCreateRoomPopup()
{
	if (!CreateRoomPopup)
	{
		return;
	}

	if (CreateRoomPopup->IsInViewport())
	{
		CreateRoomPopup->RemoveFromParent();
	}
}

void UMosesLobbyWidget::HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers)
{
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	BeginPendingEnterRoom_UIOnly();

	PC->Server_CreateRoom(RoomTitle, MaxPlayers);

	CloseCreateRoomPopup();
}

void UMosesLobbyWidget::HandleCreateRoomPopupCancel()
{
	CloseCreateRoomPopup();
}

void UMosesLobbyWidget::CenterPopupWidget(UUserWidget* PopupWidget) const
{
	if (!PopupWidget)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PopupWidget->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
	}
}

// =====================================================
// Start Game Policy
// =====================================================

bool UMosesLobbyWidget::CanStartGame_UIOnly() const
{
	const AMosesPlayerState* PS = GetMosesPS();
	if (!PS || !PS->IsRoomHost())
	{
		return false;
	}

	const AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	if (!LGS)
	{
		return false;
	}

	const FMosesLobbyRoomItem* Room = LGS->FindRoom(PS->GetRoomId());
	if (!Room)
	{
		return false;
	}

	// ✅ 정원 체크: MaxPlayers == 현재 인원
	if (Room->MaxPlayers > 0 && Room->MemberPids.Num() != Room->MaxPlayers)
	{
		return false;
	}

	return Room->IsAllReady();
}

void UMosesLobbyWidget::UpdateStartButton()
{
	if (!Button_StartGame)
	{
		return;
	}

	Button_StartGame->SetIsEnabled(!bPendingEnterRoom_UIOnly && CanStartGame_UIOnly());
}

// =====================================================
// UI Event Handlers
// =====================================================

void UMosesLobbyWidget::OnClicked_CreateRoom()
{
	OpenCreateRoomPopup();
}

void UMosesLobbyWidget::OnClicked_LeaveRoom()
{
	if (CheckBox_Ready)
	{
		CheckBox_Ready->SetIsChecked(false);
	}

	if (AMosesPlayerController* MosesPlayerController = Cast<AMosesPlayerController>(GetOwningPlayer()))
	{
		MosesPlayerController->Server_LeaveRoom();
	}
}

void UMosesLobbyWidget::OnClicked_StartGame()
{
	if (AMosesPlayerController* MosesPlayerController = Cast<AMosesPlayerController>(GetOwningPlayer()))
	{
		MosesPlayerController->Server_RequestStartMatch();
	}
}

void UMosesLobbyWidget::OnReadyChanged(bool bIsChecked)
{
	if (AMosesPlayerController* MosesPlayerController = GetMosesPC())
	{
		if (bPendingEnterRoom_UIOnly)
		{
			if (CheckBox_Ready)
			{
				CheckBox_Ready->SetIsChecked(false);
			}

			return;
		}

		MosesPlayerController->Server_SetReady(bIsChecked);
		UpdateStartButton();
	}
}

void UMosesLobbyWidget::OnClicked_CharNext()
{
	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[BTN] CharNext | Widget=%s | World=%s | OwningLP=%s | OwningPC=%s | IsLocalPC=%d | NetMode=%d"),
		*GetPathNameSafe(this),
		*GetPathNameSafe(GetWorld()),
		*GetPathNameSafe(GetOwningLocalPlayer()),
		*GetPathNameSafe(GetOwningPlayer()),
		GetOwningPlayer() ? (GetOwningPlayer()->IsLocalController() ? 1 : 0) : 0,
		GetWorld() ? (int32)GetWorld()->GetNetMode() : -1);

	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] OnClicked_CharNext LobbyLPS=%s"),
		*GetNameSafe(LobbyLPS));

	if (!LobbyLPS)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharPreview][UI] CharNext FAIL: LobbyLPS NULL (OwningLP broken?)"));
		return;
	}

	LobbyLPS->RequestNextCharacterPreview_LocalOnly();
}

void UMosesLobbyWidget::OnClicked_GameRules()
{
	UMosesLobbyLocalPlayerSubsystem* Subsys = GetLobbySubsys();
	if (!Subsys)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[BTN][GameRules] FAIL: Subsys null"));
		return;
	}

	// 1) 로컬 RulesView 연출만
	Subsys->EnterRulesView_UIOnly();

	// 2) (선택) UIOnly면 NetState 기반 강제 Apply도 끄는게 안전
	// if (LobbyLPS)
	// {
	//     const FDialogueNetState& Latest = LobbyLPS->GetLastDialogueNetState();
	//     ApplyDialogueState_ToBubbleUI(Latest);
	// }

	// 3) ❌ Day2(UIOnly)에서는 서버 진입 금지
	// Subsys->RequestEnterLobbyDialogue();
}

void UMosesLobbyWidget::OnClicked_ExitDialogue()
{
	UMosesLobbyLocalPlayerSubsystem* Subsys = GetLobbySubsys();
	if (!Subsys)
	{
		return;
	}

	Subsys->ExitRulesView_UIOnly();

	HideDialogueBubble_UI(false);
	SetSubtitleVisibility(false);
	CachedDialogueSeq = 0;
	CachedLineIndex = INDEX_NONE;
	CachedFlowState = EDialogueFlowState::Ended;
	PendingSubtitleText = FText::GetEmpty();

	return; // ✅ 여기서 끝 (UIOnly)
}

void UMosesLobbyWidget::OnClicked_SendChat()
{
	if (AMosesPlayerController* MosesPlayerController = Cast<AMosesPlayerController>(GetOwningPlayer()))
	{
		const FString Text = GetChatInput().TrimStartAndEnd();
		if (!Text.IsEmpty())
		{
			MosesPlayerController->Server_SendLobbyChat(Text);

			// UX: 전송 후 입력창 비우기
			if (ChatEditableTextBox)
			{
				ChatEditableTextBox->SetText(FText::GetEmpty());
			}
		}
	}
}

void UMosesLobbyWidget::OnRoomItemClicked(UObject* ClickedItem)
{
	const UMSLobbyRoomListItemData* Item = Cast<UMSLobbyRoomListItemData>(ClickedItem);
	if (!Item) return;

	AMosesPlayerController* PC = GetMosesPC();
	AMosesPlayerState* PS = GetMosesPS();
	if (!PC || !PS) return;

	// ✅ 로그인 안됐으면: Join 보내지 말고 로그인 유도 + "조인 예약"
	if (!PS->IsLoggedIn())
	{
		PendingJoinRoomId = Item->GetRoomId();
		bPendingJoinAfterLogin = true;

		if (LobbyLPS)
		{
			const FString AutoNick = FString::Printf(TEXT("Guest_%d"),
				GetOwningLocalPlayer() ? GetOwningLocalPlayer()->GetControllerId() : 0);

			LobbyLPS->RequestSetLobbyNickname_LocalOnly(AutoNick);
		}

		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] Join deferred until login. Room=%s"), *PendingJoinRoomId.ToString());
		return;
	}

	BeginPendingEnterRoom_UIOnly();
	PC->Server_JoinRoom(Item->GetRoomId());
}

// =====================================================
// Helpers
// =====================================================

AMosesPlayerController* UMosesLobbyWidget::GetMosesPC() const
{
	return Cast<AMosesPlayerController>(GetOwningPlayer());
}

AMosesPlayerState* UMosesLobbyWidget::GetMosesPS() const
{
	return Cast<AMosesPlayerState>(GetOwningPlayerState());
}

UMosesLobbyLocalPlayerSubsystem* UMosesLobbyWidget::GetLobbySubsys() const
{
	const APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	return LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>();
}

bool UMosesLobbyWidget::IsLocalHost() const
{
	const AMosesPlayerState* PS = GetMosesPS();
	return (PS && PS->IsRoomHost());
}

// =====================================================
// Pending Enter Room (UI Only)
// =====================================================

void UMosesLobbyWidget::BeginPendingEnterRoom_UIOnly()
{
	bPendingEnterRoom_UIOnly = true;

	RefreshAll_UI();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingEnterRoomTimerHandle);
		World->GetTimerManager().SetTimer(
			PendingEnterRoomTimerHandle,
			this,
			&UMosesLobbyWidget::OnPendingEnterRoomTimeout_UIOnly,
			PendingEnterRoomTimeoutSeconds,
			false
		);
	}
}

void UMosesLobbyWidget::EndPendingEnterRoom_UIOnly()
{
	bPendingEnterRoom_UIOnly = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingEnterRoomTimerHandle);
	}

	RefreshAll_UI();
}

void UMosesLobbyWidget::OnPendingEnterRoomTimeout_UIOnly()
{
	const AMosesPlayerState* PS = GetMosesPS();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid());

	if (!bInRoom)
	{
		EndPendingEnterRoom_UIOnly();
	}
}

// =====================================================
// Join Room Result (Subsystem → UI)
// =====================================================

void UMosesLobbyWidget::HandleJoinRoomResult_UI(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	if (Result == EMosesRoomJoinResult::Ok)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom FAIL Result=%d Room=%s"),
		(int32)Result, *RoomId.ToString());

	EndPendingEnterRoom_UIOnly();

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom FAIL MSG = %s"),
		*JoinFailReasonToText(Result));
}

FString UMosesLobbyWidget::JoinFailReasonToText(EMosesRoomJoinResult Result) const
{
	switch (Result)
	{
	case EMosesRoomJoinResult::InvalidRoomId: return TEXT("잘못된 방입니다.");
	case EMosesRoomJoinResult::NoRoom:        return TEXT("방이 존재하지 않습니다.");
	case EMosesRoomJoinResult::RoomFull:      return TEXT("방 인원이 꽉 찼습니다.");
	case EMosesRoomJoinResult::AlreadyMember: return TEXT("이미 방에 들어가 있습니다.");
	case EMosesRoomJoinResult::NotLoggedIn:   return TEXT("로그인이 필요합니다.");
	default:                                  return TEXT("입장에 실패했습니다.");
	}
}

// =====================================================
// ✅ Dialogue Bubble internals (LobbyWidget 내부 위젯 직접)
// =====================================================

void UMosesLobbyWidget::CacheDialogueBubble_InternalRefs()
{
	CachedBubbleRoot = nullptr;
	CachedTB_DialogueText = nullptr;

	if (!WidgetTree)
	{
		return;
	}

	// ✅ LobbyWidget 내부 위젯 이름을 직접 찾는다
	CachedBubbleRoot = WidgetTree->FindWidget(TEXT("BubbleRoot"));
	CachedTB_DialogueText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TB_DialogueText")));

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyUI][Bubble] Cache OK Root=%s Text=%s SizeBox=%s"),
		*GetNameSafe(CachedBubbleRoot),
		*GetNameSafe(CachedTB_DialogueText),
		*GetNameSafe(DialogueBubbleWidget));

	if (!DialogueBubbleWidget || !CachedBubbleRoot || !CachedTB_DialogueText)
	{
		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[LobbyUI][Bubble] Cache FAIL. Check BP names: DialogueBubbleWidget(SizeBox), BubbleRoot, TB_DialogueText"));
	}
}

void UMosesLobbyWidget::SetBubbleOpacity(float Opacity)
{
	// BubbleRoot가 있으면 그게 최우선(배경+텍스트 전체 페이드)
	if (CachedBubbleRoot)
	{
		CachedBubbleRoot->SetRenderOpacity(Opacity);
	}
	else if (DialogueBubbleWidget)
	{
		DialogueBubbleWidget->SetRenderOpacity(Opacity);
	}
}

void UMosesLobbyWidget::StopBubbleFade()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BubbleFadeTimerHandle);
	}
}

void UMosesLobbyWidget::StartBubbleFade(float FromOpacity, float ToOpacity, bool bCollapseAfterFade)
{
	StopBubbleFade();

	BubbleFadeElapsed = 0.f;
	BubbleFadeFrom = FromOpacity;
	BubbleFadeTo = ToOpacity;
	bBubbleCollapseAfterFade = bCollapseAfterFade;

	SetBubbleOpacity(FromOpacity);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BubbleFadeTimerHandle,
			this,
			&UMosesLobbyWidget::TickBubbleFade,
			0.016f,
			true
		);
	}
}

void UMosesLobbyWidget::TickBubbleFade()
{
	BubbleFadeElapsed += 0.016f;

	const float Alpha = FMath::Clamp(BubbleFadeElapsed / BubbleFadeDurationSeconds, 0.f, 1.f);
	const float NewOpacity = FMath::Lerp(BubbleFadeFrom, BubbleFadeTo, Alpha);

	SetBubbleOpacity(NewOpacity);

	if (Alpha >= 1.f)
	{
		StopBubbleFade();

		if (bBubbleCollapseAfterFade && DialogueBubbleWidget)
		{
			DialogueBubbleWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMosesLobbyWidget::ShowDialogueBubble_UI(bool bPlayFadeIn)
{
	if (!DialogueBubbleWidget)
	{
		return;
	}

	DialogueBubbleWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (bPlayFadeIn)
	{
		StartBubbleFade(0.f, 1.f, false);
	}
	else
	{
		StopBubbleFade();
		SetBubbleOpacity(1.f);
	}
}

void UMosesLobbyWidget::HideDialogueBubble_UI(bool bPlayFadeOut)
{
	if (!DialogueBubbleWidget)
	{
		return;
	}

	if (bPlayFadeOut)
	{
		// FadeOut 끝나면 Collapsed
		StartBubbleFade(1.f, 0.f, true);
	}
	else
	{
		StopBubbleFade();
		DialogueBubbleWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][UI] BubbleVisible=0"));
}

void UMosesLobbyWidget::SetDialogueBubbleText_UI(const FText& Text)
{
	if (CachedTB_DialogueText)
	{
		CachedTB_DialogueText->SetText(Text);
	}
}

void UMosesLobbyWidget::HandleDialogueStateChanged_UI(const FDialogueNetState& NewState)
{
	ApplyDialogueState_ToBubbleUI(NewState);
}

bool UMosesLobbyWidget::ShouldShowBubbleInCurrentMode(const FDialogueNetState& NetState) const
{
	// ✅ 정책:
	// - UI 모드(RulesView 등)와 무관하게,
	// - DialogueNetState가 "유효"하면 버블은 보여준다.
	// - Ended만 완전 종료로 취급.

	const bool bEnded = (NetState.FlowState == EDialogueFlowState::Ended);

	// LineIndex는 음수면 유효하지 않은 상태로 본다(방어)
	const bool bValidLine = (NetState.LineIndex >= 0);

	// SubState는 None이어도(WaitingInput 같은) "마지막 말풍선 유지"가 목표면 보여준다.
	return (!bEnded) && bValidLine;
}


FText UMosesLobbyWidget::GetSubtitleTextFromNetState(const FDialogueNetState& NetState) const
{
	if (DialogueLineData)
	{
		if (const FMosesDialogueLine* Line = DialogueLineData->GetLine(NetState.LineIndex))
		{
			return Line->SubtitleText;
		}
	}

	return FText::FromString(FString::Printf(TEXT("Line %d"), NetState.LineIndex));
}

void UMosesLobbyWidget::ApplyDialogueState_ToBubbleUI(const FDialogueNetState& NewState)
{
	if (!DialogueBubbleWidget)
	{
		return;
	}

	// 내부 캐시 없으면 재시도
	if (!CachedBubbleRoot || !CachedTB_DialogueText)
	{
		CacheDialogueBubble_InternalRefs();
	}

	// 1) 표시 여부 판정
	const bool bShouldShowBubble = ShouldShowBubbleInCurrentMode(NewState);

	// 텍스트 미리 생성
	const FText NewSubtitle = GetSubtitleTextFromNetState(NewState);

	// 2) 현재 UI 상태
	const bool bWasBubbleHidden =
		(DialogueBubbleWidget->GetVisibility() == ESlateVisibility::Collapsed);

	const bool bSubtitleCurrentlyHidden =
		(CachedTB_DialogueText && CachedTB_DialogueText->GetVisibility() == ESlateVisibility::Collapsed);

	// 3) 변화 감지 (캐시 갱신 전에 계산)
	const bool bSeqChanged = (CachedDialogueSeq != NewState.LastCommandSeq);
	const bool bLineChanged = (CachedLineIndex != NewState.LineIndex);
	const bool bFlowChanged = (CachedFlowState != NewState.FlowState);

	// ✅ "세션 리셋" 감지: 서버가 Seq를 다시 1부터 시작하는 등
	const bool bHasValidCache = (CachedLineIndex != INDEX_NONE);
	const bool bSeqResetDetected =
		bHasValidCache &&
		(NewState.LastCommandSeq > 0) &&
		(CachedDialogueSeq > 0) &&
		(NewState.LastCommandSeq < CachedDialogueSeq);

	// ✅ 캐시가 무효화 상태면(Exit에서 Reset 해둔 상태) 강제 Apply
	const bool bCacheInvalid = (CachedLineIndex == INDEX_NONE) || (CachedDialogueSeq == 0);

	// ---------------------------------------------------------
	// 4) 버블을 보여주지 않는 상태면: UI 내리고 캐시도 무효화
	// ---------------------------------------------------------
	if (!bShouldShowBubble)
	{
		// 다음 세션에서 반드시 다시 그리도록 무효화
		CachedDialogueSeq = 0;
		CachedLineIndex = INDEX_NONE;
		CachedFlowState = EDialogueFlowState::Ended;
		PendingSubtitleText = FText::GetEmpty();

		SetSubtitleVisibility(false);
		HideDialogueBubble_UI(true);
		return;
	}

	// ---------------------------------------------------------
	// 5) 버블은 보여준다
	// ---------------------------------------------------------
	if (NewState.SubState != EDialogueSubState::None)
	{
		ShowDialogueBubble_UI(bWasBubbleHidden);
	}

	// 메타휴먼 강제 Collapsed 상태면 Pending만 갱신
	if (bSubtitleForcedCollapsedByMetaHuman)
	{
		PendingSubtitleText = NewSubtitle;
		return;
	}

	SetSubtitleVisibility(true);

	// ---------------------------------------------------------
	// 6) ✅ 텍스트 강제 적용 조건
	// ---------------------------------------------------------
	const bool bHasPending = !PendingSubtitleText.IsEmpty();
	const bool bRecoveredFromHidden = (bWasBubbleHidden || bSubtitleCurrentlyHidden);

	const bool bForceApply =
		bRecoveredFromHidden ||
		bHasPending ||
		bSeqResetDetected ||
		bCacheInvalid;

	// 7) 텍스트 갱신
	if (bForceApply || bLineChanged || bSeqChanged)
	{
		const FText TextToApply = bHasPending ? PendingSubtitleText : NewSubtitle;
		PendingSubtitleText = FText::GetEmpty();

		SetDialogueBubbleText_UI(TextToApply);

		UE_LOG(LogMosesSpawn, Log,
			TEXT("[DOD][UI] BubbleVisible=1 Text='%s' line=%d Flow=%d Sub=%d Seq=%d (Apply=1 Force=%d Reset=%d Invalid=%d)"),
			*TextToApply.ToString(),
			NewState.LineIndex,
			(int32)NewState.FlowState,
			(int32)NewState.SubState,
			NewState.LastCommandSeq,
			bForceApply ? 1 : 0,
			bSeqResetDetected ? 1 : 0,
			bCacheInvalid ? 1 : 0
		);
	}

	// 8) 캐시 갱신은 마지막에
	CachedDialogueSeq = NewState.LastCommandSeq;
	CachedLineIndex = NewState.LineIndex;
	CachedFlowState = NewState.FlowState;

	(void)bFlowChanged;
}

// =====================================================
// Public UI Mode Control
// =====================================================

void UMosesLobbyWidget::SetRulesViewMode(bool bEnable)
{
	if (bRulesViewEnabled == bEnable)
	{
		UE_LOG(LogMosesSpawn, VeryVerbose, TEXT("[LobbyUI] SetRulesViewMode SKIP Already=%d"),
			bEnable ? 1 : 0);
		return;
	}

	bRulesViewEnabled = bEnable;

	auto SetVisSafe = [](UWidget* W, ESlateVisibility Vis)
		{
			if (W)
			{
				W->SetVisibility(Vis);
			}
		};

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyUI] SetRulesViewMode APPLY=%d Bubble=%s"),
		bEnable ? 1 : 0, *GetNameSafe(DialogueBubbleWidget));

	if (bEnable)
	{
		// ✅ RulesView ON: 패널만 숨김
		SetVisSafe(LeftPanel, ESlateVisibility::Collapsed);
		SetVisSafe(RightPanel, ESlateVisibility::Collapsed);
		SetVisSafe(CharacterSelectedButtonsBox, ESlateVisibility::Collapsed);
		SetVisSafe(Button_StartGame, ESlateVisibility::Collapsed);
		SetVisSafe(ClientPanel, ESlateVisibility::Collapsed);

		SetVisSafe(Btn_ExitDialogue, ESlateVisibility::Visible);

		// ✅ 하단 UI(내 발화/마이크 상태) 표시
		// - 기본은 Collapsed로 두고 RulesView에서만 노출
		SetVisSafe(TB_MySpeech, ESlateVisibility::Visible);
		SetVisSafe(TB_MicState, ESlateVisibility::Visible);

		// ✅ Bubble은 여기서 건드리지 않는다.
		// - 강제 Collapsed/Opacity0 제거
		// - 대화 상태에 따라 ApplyDialogueState_ToBubbleUI가 show/hide 담당

		// ✅ RulesView 진입 순간: 최신 NetState로 즉시 반영(보여야 하면 바로 보임)
		if (LobbyLPS)
		{
			HandleDialogueStateChanged_UI(LobbyLPS->GetLastDialogueNetState());

			// ✅ Day2: 세션(하단 텍스트/마이크 상태)도 즉시 반영
			// (함수명이 다르면 네 위젯/서브시스템에 맞춰 연결)
			SetMySpeechText_UI(LobbyLPS->GetCachedMySpeechText());   // 없으면 아래 주석 방식으로
			SetMicState_UI(static_cast<int32>(LobbyLPS->GetMicState()));
		}
		else
		{
			// LobbyLPS가 아직 없을 수도 있으니 기본 값 표시
			SetMySpeechText_UI(FText::GetEmpty());
			SetMicState_UI(/*Off*/0);
		}
	}
	else
	{
		// ✅ RulesView OFF: 로비 패널 복구
		SetVisSafe(LeftPanel, ESlateVisibility::SelfHitTestInvisible);
		SetVisSafe(RightPanel, ESlateVisibility::SelfHitTestInvisible);
		SetVisSafe(CharacterSelectedButtonsBox, ESlateVisibility::SelfHitTestInvisible);

		SetVisSafe(Btn_ExitDialogue, ESlateVisibility::Collapsed);

		// ✅ 하단 UI 숨김
		SetVisSafe(TB_MySpeech, ESlateVisibility::Collapsed);
		SetVisSafe(TB_MicState, ESlateVisibility::Collapsed);

		// ✅ Bubble은 여기서도 건드리지 않는다.
		// (대화가 진행 중이면 계속 보여야 함)
	}

	RefreshGameRulesButtonVisibility();
}


void UMosesLobbyWidget::RefreshFromState(const AMosesLobbyGameState* InMosesLobbyGameState, const AMosesPlayerState* InMosesPlayerState)
{
	// 내 닉 표시
	if (NickNameText && InMosesPlayerState)
	{
		FString NickName = InMosesPlayerState->GetPlayerNickName();

		// ✅ fallback: 아직 Nick이 복제되기 전이면 PlayerName이라도 표시
		if (NickName.IsEmpty())
		{
			NickName = InMosesPlayerState->GetPlayerName().TrimStartAndEnd();
		}

		if (NickName.IsEmpty())
		{
			NickName = TEXT("Guest");
		}

		NickNameText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		NickNameText->SetText(FText::FromString(NickName));
	}

	// 내 방 표시
	if (TXT_MyRoom && InMosesPlayerState)
	{
		const FString RoomStr = InMosesPlayerState->GetRoomId().IsValid()
			? InMosesPlayerState->GetRoomId().ToString(EGuidFormats::DigitsWithHyphens)
			: TEXT("None");
		TXT_MyRoom->SetText(FText::FromString(RoomStr));
	}

	// 채팅 재구축(최소 구현: 전체 clear 후 다시 add)
	RebuildChat(InMosesLobbyGameState);

	// Start는 Host만 UX 활성(최종 검증은 서버가 한다)
	RefreshRightPanelControlsByRole();
		
	// (선택) Host는 Ready 버튼 비활성 같은 UX를 원하면 여기서 처리 가능
	// if (BTN_Ready && MyPS) { BTN_Ready->SetIsEnabled(!MyPS->IsRoomHost()); }
}


void UMosesLobbyWidget::RefreshGameRulesButtonVisibility()
{
	if (!Btn_GameRules)
	{
		return;
	}

	const bool bShouldShow =
		(!bPendingEnterRoom_UIOnly)
		&& (!bRulesViewEnabled);

	const ESlateVisibility NewVis = bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	Btn_GameRules->SetVisibility(NewVis);
	Btn_GameRules->SetIsEnabled(bShouldShow);
}

FString UMosesLobbyWidget::GetChatInput() const
{
	return ChatEditableTextBox ? ChatEditableTextBox->GetText().ToString() : FString();
}

void UMosesLobbyWidget::RebuildChat(const AMosesLobbyGameState* GS)
{
	if (!ChatListView)
	{
		return;
	}

	ChatListView->ClearListItems();

	if (!GS)
	{
		return;
	}

	// ✅ GS가 가진 ChatHistory를 그대로 UI 리스트로 재생성
	for (const FLobbyChatMessage& Msg : GS->GetChatHistory())
	{
		const FString Line = FString::Printf(TEXT("%s: %s"), *Msg.SenderName, *Msg.Message);
		ChatListView->AddItem(UMosesLobbyChatRowItem::Make(this, Line));
	}
}

void UMosesLobbyWidget::SetSubtitleVisibility(bool bVisible)
{
	if (!CachedTB_DialogueText)
	{
		return;
	}

	CachedTB_DialogueText->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMosesLobbyWidget::OnEnterMetaHumanView_SubtitleOnly()
{
	// ✅ 메타휴먼 시점 들어갈 때: 자막 텍스트만 강제 내린다.
	// (버블/루트 전체는 네 기존 RulesView 정책대로 두고 싶으면 건드리지 말기)
	if (!CachedTB_DialogueText)
	{
		CacheDialogueBubble_InternalRefs();
	}

	if (CachedTB_DialogueText)
	{
		SetSubtitleVisibility(false);
		bSubtitleForcedCollapsedByMetaHuman = true;
	}
}

void UMosesLobbyWidget::OnExitMetaHumanView_SubtitleOnly_Reapply(const FDialogueNetState& LatestState)
{
	// ✅ 메타휴먼 시점에서 돌아왔을 때:
	// - 자막 텍스트 Visible 복구
	// - 최신 상태를 1회 강제 Apply 해서 “텍스트가 다음 줄로 안 바뀜”을 종결한다.
	if (!CachedTB_DialogueText)
	{
		CacheDialogueBubble_InternalRefs();
	}

	if (bSubtitleForcedCollapsedByMetaHuman)
	{
		SetSubtitleVisibility(true);
		bSubtitleForcedCollapsedByMetaHuman = false;
	}

	// 복귀 직후 1회 강제 재적용 (Pending 처리 포함)
	ApplyDialogueState_ToBubbleUI(LatestState);
}

void UMosesLobbyWidget::BindVoiceSubsystemEvents()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP) return;

	if (UMosesLobbyVoiceSubsystem* Voice = LP->GetSubsystem<UMosesLobbyVoiceSubsystem>())
	{
		Voice->OnVoiceLevelChanged().RemoveAll(this);
		Voice->OnVoiceLevelChanged().AddUObject(this, &UMosesLobbyWidget::HandleVoiceLevelChanged);

		// 초기값 즉시 반영
		HandleVoiceLevelChanged(Voice->GetVoiceLevel01());
	}
}

void UMosesLobbyWidget::UnbindVoiceSubsystemEvents()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP) return;

	if (UMosesLobbyVoiceSubsystem* Voice = LP->GetSubsystem<UMosesLobbyVoiceSubsystem>())
	{
		Voice->OnVoiceLevelChanged().RemoveAll(this);
	}
}

void UMosesLobbyWidget::OnClicked_MicToggle()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP) return;

	if (UMosesLobbyVoiceSubsystem* Voice = LP->GetSubsystem<UMosesLobbyVoiceSubsystem>())
	{
		const bool bNewEnable = !Voice->IsMicEnabled();
		Voice->SetMicEnabled(bNewEnable);
	}
}

void UMosesLobbyWidget::HandleVoiceLevelChanged(float Level01)
{
	if (PB_MicLevel)
	{
		PB_MicLevel->SetPercent(FMath::Clamp(Level01, 0.f, 1.f));
	}

	UE_LOG(LogTemp, Log, TEXT("[UI] MicLevel=%.2f"), Level01);
}

void UMosesLobbyWidget::OnClicked_SubmitCommand()
{
	if (!LobbyLPS)
	{
		return;
	}

	const FString Text = ET_CommandInput ? ET_CommandInput->GetText().ToString() : TEXT("");
	if (Text.IsEmpty())
	{
		return;
	}

	LobbyLPS->SubmitCommandText(Text);

	if (ET_CommandInput)
	{
		ET_CommandInput->SetText(FText::GetEmpty());
	}
}

void UMosesLobbyWidget::SetMySpeechText_UI(const FText& InText)
{
	if (TB_MySpeech)
	{
		TB_MySpeech->SetText(InText);
	}
}

void UMosesLobbyWidget::SetMicState_UI(int32 InMicState)
{
	if (!TB_MicState)
	{
		return;
	}

	// Day2: 단순 표시
	switch ((EMosesMicState)InMicState)
	{
	case EMosesMicState::Off:        TB_MicState->SetText(FText::FromString(TEXT("Mic: Off"))); break;
	case EMosesMicState::Listening:  TB_MicState->SetText(FText::FromString(TEXT("Mic: Listening"))); break;
	case EMosesMicState::Processing: TB_MicState->SetText(FText::FromString(TEXT("Mic: Processing"))); break;
	case EMosesMicState::Error:      TB_MicState->SetText(FText::FromString(TEXT("Mic: Error"))); break;
	default:                         TB_MicState->SetText(FText::FromString(TEXT("Mic: Unknown"))); break;
	}
}
