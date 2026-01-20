// ============================================================================
// MosesLobbyWidget.cpp
// ============================================================================

#include "MosesLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/ListView.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanelSlot.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/UI/Lobby/MSCreateRoomPopupWidget.h"
#include "UE5_Multi_Shooter/UI/Lobby/MSLobbyRoomListviewEntryWidget.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyChatRowItem.h"

namespace MosesLobbyWidget_Internal
{
	FORCEINLINE const TCHAR* NetModeToString(ENetMode Mode)
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
}

// =====================================================
// Engine Lifecycle
// =====================================================

void UMosesLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 가시성(안전)
	if (RightPanel)
	{
		RightPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (LeftPanel)
	{
		LeftPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (RoomListViewOverlay)
	{
		RoomListViewOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Subsystem 캐시 + 바인딩
	CacheLocalSubsystem();
	BindLobbyButtons();
	BindListViewEvents();
	BindSubsystemEvents();

	// 채팅 입력(Enter 전송) 바인딩
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::BindChatEvents_NextTick);
	}

	// 최초 갱신
	RefreshAll_UI();

	// Subsystem에 자신 등록(Subsystem이 UI 상태를 “단일 갱신 파이프”로 쓰는 경우 대비)
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* LPS = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			LPS->RegisterLobbyWidget(this);
		}
	}
}

void UMosesLobbyWidget::NativeDestruct()
{
	// Delegate/Timer 정리 (유령 바인딩 방지)
	UnbindSubsystemEvents();

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

	if (ChatEditText)
	{
		ChatEditText->OnTextCommitted.RemoveAll(this);
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

	// 중복 바인딩 방지
	LobbyLPS->OnLobbyPlayerStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyRoomStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyJoinRoomResult().RemoveAll(this);

	LobbyLPS->OnLobbyPlayerStateChanged().AddUObject(this, &UMosesLobbyWidget::HandlePlayerStateChanged_UI);
	LobbyLPS->OnLobbyRoomStateChanged().AddUObject(this, &UMosesLobbyWidget::HandleRoomStateChanged_UI);
	LobbyLPS->OnLobbyJoinRoomResult().AddUObject(this, &UMosesLobbyWidget::HandleJoinRoomResult_UI);
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
}

// =====================================================
// Unified UI Refresh
// =====================================================

void UMosesLobbyWidget::RefreshAll_UI()
{
	RefreshPanelsByPlayerState();
	RefreshRightPanelControlsByRole();

	// 방 밖일 때만 방 리스트 갱신(깜빡임 방지 정책 유지)
	const AMosesPlayerState* PS = GetMosesPS();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid()) || bPendingEnterRoom_UIOnly;
	if (!bInRoom)
	{
		RefreshRoomListFromGameState();
	}

	UpdateStartButton();
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

	const AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
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

	RoomListView->ClearListItems();

	for (const FMosesLobbyRoomItem& Room : LGS->GetRooms())
	{
		UMSLobbyRoomListviewEntryData* Item = NewObject<UMSLobbyRoomListviewEntryData>(this);
		Item->Init(Room.RoomId, Room.RoomTitle, Room.MaxPlayers, Room.MemberPids.Num());
		RoomListView->AddItem(Item);
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = GetOwningPlayer();

	UE_LOG(LogTemp, Warning, TEXT("[RoomUI] World=%s NetMode=%s PCAuth=%d Rooms=%d HasAny=%d InRoom_UIOnly=%d"),
		*GetNameSafe(World),
		World ? MosesLobbyWidget_Internal::NetModeToString(World->GetNetMode()) : TEXT("None"),
		(PC && PC->HasAuthority()) ? 1 : 0,
		LGS->GetRooms().Num(),
		bHasAnyRoom ? 1 : 0,
		bInRoom_UIOnly ? 1 : 0);
}

void UMosesLobbyWidget::RefreshPanelsByPlayerState()
{
	const AMosesPlayerState* PS = GetMosesPS();

	const bool bInRoomByRep = (PS && PS->GetRoomId().IsValid());
	const bool bInRoom_UIOnly = bInRoomByRep || bPendingEnterRoom_UIOnly;

	const ESlateVisibility NewLeft = bInRoom_UIOnly ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
	const ESlateVisibility NewRight = bInRoom_UIOnly ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	if (LeftPanel && LeftPanel->GetVisibility() != NewLeft)
	{
		if (Button_CreateRoom)
		{
			Button_CreateRoom->SetVisibility((NewLeft == ESlateVisibility::SelfHitTestInvisible) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}

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

	// Pending 중엔 “Role UI” 잠깐도 보여주지 않음(깜빡임 방지)
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

		// 방 밖/전환중/호스트면 체크는 항상 false
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

	// [MOD] 호스트도 채팅을 쳐야 하므로 ClientPanel을 호스트에게도 숨기지 않는다.
	// - ClientPanel 안에 "Ready 같은 클라 전용 UI"가 있어도, 위에서 이미 CheckBox_Ready는 Host에겐 숨기고 있음.
	// - 따라서 Panel 자체는 보여줘도 호스트에게 불필요한 컨트롤은 이미 숨김/비활성화 상태로 유지됨.
	if (ClientPanel)
	{
		ClientPanel->SetVisibility(bAllowRoleSpecificUI ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 채팅 입력/전송 버튼도 “방 안 + Pending 아님”에서만 허용(UX 게이트)
	const bool bCanChat = bAllowRoleSpecificUI && CanSendChat_UIOnly();
	if (ChatEditText)
	{
		ChatEditText->SetIsEnabled(bCanChat);
	}
	if (BTN_SendChat)
	{
		BTN_SendChat->SetIsEnabled(bCanChat);
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

	// “로그인 이후 자동 Join” 예약 처리
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

	// Rep로 방 진입이 확인되면 Pending 종료
	if (PS && PS->GetRoomId().IsValid() && bPendingEnterRoom_UIOnly)
	{
		EndPendingEnterRoom_UIOnly();
		return;
	}

	RefreshAll_UI();
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
			UMSCreateRoomPopupWidget::FOnConfirmCreateRoom::CreateUObject(this, &UMosesLobbyWidget::HandleCreateRoomPopupConfirm));

		CreateRoomPopup->BindOnCancel(
			UMSCreateRoomPopupWidget::FOnCancelCreateRoom::CreateUObject(this, &UMosesLobbyWidget::HandleCreateRoomPopupCancel));
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

	// 정원 체크: MaxPlayers == 현재 인원
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

	if (AMosesPlayerController* PC = GetMosesPC())
	{
		PC->Server_LeaveRoom();
	}
}

void UMosesLobbyWidget::OnClicked_StartGame()
{
	if (AMosesPlayerController* PC = GetMosesPC())
	{
		PC->Server_RequestStartMatch();
	}
}

void UMosesLobbyWidget::OnReadyChanged(bool bIsChecked)
{
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	// Pending 상태에서는 Ready 입력 무시(UX 보호)
	if (bPendingEnterRoom_UIOnly)
	{
		if (CheckBox_Ready)
		{
			CheckBox_Ready->SetIsChecked(false);
		}
		return;
	}

	PC->Server_SetReady(bIsChecked);
	UpdateStartButton();
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

	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] OnClicked_CharNext LobbyLPS=%s"), *GetNameSafe(LobbyLPS));

	if (!LobbyLPS)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharPreview][UI] CharNext FAIL: LobbyLPS NULL (OwningLP broken?)"));
		return;
	}

	LobbyLPS->RequestNextCharacterPreview_LocalOnly();
}

void UMosesLobbyWidget::OnClicked_SendChat()
{
	// [MOD] 버튼 전송도 Enter 전송과 동일한 파이프로 통일
	TrySendChatFromInput();
}

void UMosesLobbyWidget::OnChatTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// [ADD] Enter(=OnEnter)일 때만 전송.
	// - 포커스 이동/커밋(예: OnUserMovedFocus)으로도 호출될 수 있으니 반드시 필터링한다.
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	TrySendChatFromInput();
}

void UMosesLobbyWidget::OnRoomItemClicked(UObject* ClickedItem)
{
	const UMSLobbyRoomListviewEntryData* Item = Cast<UMSLobbyRoomListviewEntryData>(ClickedItem);
	if (!Item)
	{
		return;
	}

	AMosesPlayerController* PC = GetMosesPC();
	AMosesPlayerState* PS = GetMosesPS();
	if (!PC || !PS)
	{
		return;
	}

	// 로그인 안 됐으면: Join RPC 금지 + “조인 예약”
	if (!PS->IsLoggedIn())
	{
		PendingJoinRoomId = Item->GetRoomId();
		bPendingJoinAfterLogin = true;

		if (LobbyLPS)
		{
			const FString NickName = PS->GetPlayerNickName();
			LobbyLPS->RequestSetLobbyNickname_LocalOnly(NickName);
		}

		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] Join deferred until login. Room=%s"), *PendingJoinRoomId.ToString());
		return;
	}

	BeginPendingEnterRoom_UIOnly();
	PC->Server_JoinRoom(Item->GetRoomId());
}

// =====================================================
// Chat Helpers
// =====================================================

FString UMosesLobbyWidget::GetChatInput() const
{
	return ChatEditText ? ChatEditText->GetText().ToString() : FString();
}

bool UMosesLobbyWidget::CanSendChat_UIOnly() const
{
	const AMosesPlayerState* PS = GetMosesPS();
	if (!PS)
	{
		return false;
	}

	// [MOD] 호스트는 로그인 플래그가 false여도 채팅 허용(원하면)
	//       단, 서버도 동일 정책으로 풀어줘야 실제 전송됨.
	const bool bIsHost = PS->IsRoomHost();

	if (!bIsHost && !PS->IsLoggedIn())
	{
		return false;
	}

	if (!PS->GetRoomId().IsValid())
	{
		return false;
	}

	if (bPendingEnterRoom_UIOnly)
	{
		return false;
	}

	return true;
}

void UMosesLobbyWidget::BindChatEvents_NextTick()
{
	// [ADD] 중복 호출 방지
	if (bChatEventsBound)
	{
		return;
	}
	bChatEventsBound = true;

	if (!IsValid(ChatEditText))
	{
		return;
	}

	// [MOD] RemoveAll은 여기서도 굳이 필요 없음.
	//      AddUniqueDynamic만으로 “중복 바인딩” 문제를 막는다.
	ChatEditText->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::OnChatTextCommitted);
}

void UMosesLobbyWidget::TrySendChatFromInput()
{
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	if (!CanSendChat_UIOnly())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Chat][UI] Send blocked (NotReadyToChat)"));
		return;
	}

	const FString Text = GetChatInput().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	// ✅ 서버 권위: 클라는 "요청"만 한다.
	PC->Server_SendLobbyChat(Text);

	// UX: 전송 후 입력창 비우기
	if (ChatEditText)
	{
		ChatEditText->SetText(FText::GetEmpty());

		// [ADD] Enter 전송 후에도 계속 타이핑할 수 있게 포커스 유지(선택)
		ChatEditText->SetKeyboardFocus();
	}
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

	for (const FLobbyChatMessage& Msg : GS->GetChatHistory())
	{
		UMosesLobbyChatRowItem* Item = UMosesLobbyChatRowItem::Make(this, Msg);
		if (!Item)
		{
			continue;
		}

		ChatListView->AddItem(Item);
	}

	// [MOD] UE5.3 UListView에는 ScrollItemIntoView가 없어서 제거.
	//      (자동 스크롤은 추후, 버전 호환 방식으로 별도 구현)
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
			false);
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
// Public API (Subsystem → Widget)
// =====================================================

void UMosesLobbyWidget::RefreshFromState(const AMosesLobbyGameState* InMosesLobbyGameState, const AMosesPlayerState* InMosesPlayerState)
{
	// 내 닉 표시
	if (NickNameText && InMosesPlayerState)
	{
		FString NickName = InMosesPlayerState->GetPlayerNickName();

		// fallback: 아직 Nick이 복제되기 전이면 PlayerName이라도 표시
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

	// Host/Client 컨트롤 갱신
	RefreshRightPanelControlsByRole();
}
