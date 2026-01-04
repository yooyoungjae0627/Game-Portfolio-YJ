// MosesLobbyWidget.cpp

#include "MosesLobbyWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ListView.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/UI/Lobby/MSCreateRoomPopupWidget.h"
#include "UE5_Multi_Shooter/UI/Lobby/MSLobbyRoomItemWidget.h"

// ---------------------------
// Engine
// ---------------------------

void UMosesLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 개발자 주석:
	// - OwningPlayer/LocalPlayer가 NULL이면 CreateWidget을 GetWorld로 만든 케이스 가능성이 높다.
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] NativeConstruct OwningPlayer=%s OwningLP=%s World=%s"),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningLocalPlayer()),
		*GetNameSafe(GetWorld()));

	CacheLocalSubsystem();
	BindLobbyButtons();
	BindListViewEvents();
	BindSubsystemEvents();

	// 개발자 주석:
	// - 초기 동기화는 1회로 통일 (중복 호출 금지)
	RefreshAll_UI();
}

void UMosesLobbyWidget::NativeDestruct()
{
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

	if (Btn_CharPrev)
	{
		Btn_CharPrev->OnClicked.RemoveAll(this);
	}

	if (Btn_CharNext)
	{
		Btn_CharNext->OnClicked.RemoveAll(this);
	}

	LobbyLPS = nullptr;

	CloseCreateRoomPopup();

	Super::NativeDestruct();
}

// ---------------------------
// Setup / Bind
// ---------------------------

void UMosesLobbyWidget::CacheLocalSubsystem()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] CacheLocalSubsystem OwningLP=%s"),
		*GetNameSafe(LP));

	LobbyLPS = LP ? LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>() : nullptr;

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] CacheLocalSubsystem LobbyLPS=%s"),
		*GetNameSafe(LobbyLPS));
}

void UMosesLobbyWidget::BindLobbyButtons()
{
	// Create
	if (Button_CreateRoom)
	{
		Button_CreateRoom->OnClicked.RemoveAll(this);
		Button_CreateRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CreateRoom);
	}

	// Leave
	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->OnClicked.RemoveAll(this);
		Button_LeaveRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_LeaveRoom);
	}

	// Start
	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.RemoveAll(this);
		Button_StartGame->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_StartGame);
	}

	// Ready
	if (CheckBox_Ready)
	{
		CheckBox_Ready->OnCheckStateChanged.RemoveAll(this);
		CheckBox_Ready->OnCheckStateChanged.AddDynamic(this, &UMosesLobbyWidget::OnReadyChanged);
	}

	// Character Preview
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] BindCharacterButtons Prev=%s Next=%s"),
		*GetNameSafe(Btn_CharPrev),
		*GetNameSafe(Btn_CharNext));

	if (Btn_CharPrev)
	{
		Btn_CharPrev->OnClicked.RemoveAll(this);
		Btn_CharPrev->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CharPrev);
	}

	if (Btn_CharNext)
	{
		Btn_CharNext->OnClicked.RemoveAll(this);
		Btn_CharNext->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CharNext);
	}
}

void UMosesLobbyWidget::BindListViewEvents()
{
	if (!RoomListView)
	{
		return;
	}

	RoomListView->OnItemClicked().RemoveAll(this);
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

// ---------------------------
// Unified Refresh
// ---------------------------

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
}

// ---------------------------
// Lobby UI refresh internals
// ---------------------------

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

	RoomListView->ClearListItems();

	for (const FMosesLobbyRoomItem& Room : LGS->GetRooms())
	{
		UMSLobbyRoomListItemData* Item = NewObject<UMSLobbyRoomListItemData>(this);
		Item->Init(Room.RoomId, Room.RoomTitle, Room.MaxPlayers, Room.MemberPids.Num(), EMSLobbyRoomItemWidgetType::LeftPanel);
		RoomListView->AddItem(Item);
	}
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

	// ✅ Start 버튼: 호스트만
	if (Button_StartGame)
	{
		Button_StartGame->SetVisibility((bInRoom && bIsHost) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// ✅ Ready: 호스트 제외
	if (CheckBox_Ready)
	{
		const bool bShowReady = (bInRoom && !bIsHost);
		CheckBox_Ready->SetVisibility(bShowReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		CheckBox_Ready->SetIsEnabled(bInRoom && !bIsHost && !bPendingEnterRoom_UIOnly);
	}

	// ✅ Leave: 방 안이면 표시
	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->SetVisibility(bInRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_LeaveRoom->SetIsEnabled(bInRoom && !bPendingEnterRoom_UIOnly);
	}

	// ✅✅✅ 요구사항: Host/ClientPanel 정책
	// Host(방 생성자): ClientPanel Collapsed
	// 참가 클라:        ClientPanel SelfHitTestInvisible
	if (ClientPanel)
	{
		if (!bInRoom)
		{
			// 방 밖이면 의미 없으니 안전하게 닫음
			ClientPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ClientPanel->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

// ---------------------------
// UI Refresh Entry (Subsystem에서 호출)
// ---------------------------

void UMosesLobbyWidget::HandleRoomStateChanged_UI()
{
	// 개발자 주석:
	// - Room 목록/멤버 변경 같은 전광판 이벤트
	RefreshAll_UI();
}

void UMosesLobbyWidget::HandlePlayerStateChanged_UI()
{
	// 개발자 주석:
	// - PS.RoomId 복제 도착이 "방 입장 확정" 타이밍이다.
	// - 이때만 Pending을 해제한다 (Ok 수신 시점에는 절대 해제 금지)
	const AMosesPlayerState* PS = GetMosesPS();
	if (PS && PS->GetRoomId().IsValid() && bPendingEnterRoom_UIOnly)
	{
		EndPendingEnterRoom_UIOnly();
		// EndPending에서 RefreshAll을 호출하므로 여기서 추가 Refresh는 불필요
		return;
	}

	RefreshAll_UI();
}

// ---------------------------
// Create Room Popup
// ---------------------------

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

// ---------------------------
// Start button policy
// ---------------------------

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

	// 개발자 주석:
	// - 서버 정책과 동일해야 한다.
	// - "호스트 제외" 전원 Ready 이면 Start 가능
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

// ---------------------------
// UI Event handlers
// ---------------------------

void UMosesLobbyWidget::OnClicked_CreateRoom()
{
	OpenCreateRoomPopup();
}

void UMosesLobbyWidget::OnClicked_LeaveRoom()
{
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	PC->Server_LeaveRoom();
}

void UMosesLobbyWidget::OnClicked_StartGame()
{
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	PC->Server_RequestStartGame();
}

void UMosesLobbyWidget::OnReadyChanged(bool bIsChecked)
{
	// 개발자 주석:
	// - Pending 중에는 Ready 조작 금지 (UI만 방 안처럼 보이는 상태)
	if (bPendingEnterRoom_UIOnly)
	{
		if (CheckBox_Ready)
		{
			CheckBox_Ready->SetIsChecked(false);
		}
		return;
	}

	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	PC->Server_SetReady(bIsChecked);
	UpdateStartButton();
}

void UMosesLobbyWidget::OnClicked_CharPrev()
{
	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] OnClicked_CharPrev LobbyLPS=%s"),
		*GetNameSafe(LobbyLPS));

	if (!LobbyLPS)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharPreview][UI] CharPrev FAIL: LobbyLPS NULL (OwningLP broken?)"));
		return;
	}

	LobbyLPS->RequestPrevCharacterPreview_LocalOnly();
}

void UMosesLobbyWidget::OnClicked_CharNext()
{
	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] OnClicked_CharNext LobbyLPS=%s"),
		*GetNameSafe(LobbyLPS));

	if (!LobbyLPS)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharPreview][UI] CharNext FAIL: LobbyLPS NULL (OwningLP broken?)"));
		return;
	}

	LobbyLPS->RequestNextCharacterPreview_LocalOnly();
}

void UMosesLobbyWidget::OnRoomItemClicked(UObject* ClickedItem)
{
	const UMSLobbyRoomListItemData* Item = Cast<UMSLobbyRoomListItemData>(ClickedItem);
	if (!Item)
	{
		return;
	}

	// 개발자 주석:
	// - 클라 1차 프리체크 (방 가득 찼으면 RPC 보내지 않음)
	const AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	if (LGS)
	{
		const FMosesLobbyRoomItem* Room = LGS->FindRoom(Item->GetRoomId());
		if (Room && Room->IsFull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom REJECT (ClientPrecheck: RoomFull) Room=%s"),
				*Item->GetRoomId().ToString());
			return;
		}
	}

	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	BeginPendingEnterRoom_UIOnly();
	PC->Server_JoinRoom(Item->GetRoomId());
}

// ---------------------------
// Helpers
// ---------------------------

AMosesPlayerController* UMosesLobbyWidget::GetMosesPC() const
{
	return Cast<AMosesPlayerController>(GetOwningPlayer());
}

AMosesPlayerState* UMosesLobbyWidget::GetMosesPS() const
{
	// 개발자 주석:
	// - UE 버전에 따라 GetOwningPlayerState<T>()가 없거나 템플릿이 꼬이는 케이스가 있다.
	// - 항상 안전하게 Cast로 통일한다.
	return Cast<AMosesPlayerState>(GetOwningPlayerState());
}

// ---------------------------
// Pending Enter Room (UI Only)
// ---------------------------

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

// ---------------------------
// JoinRoom Result UI
// ---------------------------

void UMosesLobbyWidget::HandleJoinRoomResult_UI(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	// 개발자 주석:
	// - Ok = 서버 승인. 최종 전환은 PS.RoomId 복제 도착으로 확정.
	// - Ok에서 Pending을 풀면 Rep 도착 전 순간에 Left로 되돌아가 "깜빡임"이 생긴다.
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
