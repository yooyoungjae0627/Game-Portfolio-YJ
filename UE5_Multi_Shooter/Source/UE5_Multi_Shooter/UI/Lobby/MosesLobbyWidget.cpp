#include "MosesLobbyWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ListView.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanelSlot.h"

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
	// - 여기서 OwningPlayer/LocalPlayer가 NULL이면
	//   CreateWidget을 GetWorld로 만든 케이스 가능성이 높다.
	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] NativeConstruct OwningPlayer=%s OwningLP=%s World=%s"),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningLocalPlayer()),
		*GetNameSafe(GetWorld()));

	CacheLocalSubsystem();
	BindLobbyButtons();
	BindCharacterButtons();
	BindListViewEvents();
	BindSubsystemEvents();

	// 개발자 주석:
	// - 위젯 생성 직후 "초기 1회" 동기화.
	HandlePlayerStateChanged_UI();
	HandleRoomStateChanged_UI();
	UpdateStartButton();
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

	// 개발자 주석:
	// - 캐릭터 프리뷰 버튼 바인딩 해제
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

	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] CacheLocalSubsystem OwningLP=%s"),
		*GetNameSafe(LP));

	LobbyLPS = LP ? LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>() : nullptr;

	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] CacheLocalSubsystem LobbyLPS=%s"),
		*GetNameSafe(LobbyLPS));
}

void UMosesLobbyWidget::BindLobbyButtons()
{
	if (Button_CreateRoom)
	{
		Button_CreateRoom->OnClicked.RemoveAll(this);
		Button_CreateRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CreateRoom);
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->OnClicked.RemoveAll(this);
		Button_LeaveRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_LeaveRoom);
	}

	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.RemoveAll(this);
		Button_StartGame->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_StartGame);
	}

	if (CheckBox_Ready)
	{
		CheckBox_Ready->OnCheckStateChanged.RemoveAll(this);
		CheckBox_Ready->OnCheckStateChanged.AddDynamic(this, &UMosesLobbyWidget::OnReadyChanged);
	}

	// 개발자 주석:
	// - 캐릭터 프리뷰 버튼(선택 화면의 ← / →)
	// - WBP에서 IsVariable 체크 + 이름 일치가 필수
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

void UMosesLobbyWidget::BindCharacterButtons()
{
	// 개발자 주석:
	// - 여기서 Btn_CharPrev/Next가 NULL이면:
	//   1) WBP 이름 불일치
	//   2) IsVariable 체크 안됨
	//   3) ParentClass가 UMosesLobbyWidget이 아님
	UE_LOG(LogTemp, Warning, TEXT("[CharPreview][UI] BindCharacterButtons Prev=%s Next=%s"),
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

	LobbyLPS->OnLobbyPlayerStateChanged().AddUObject(this, &UMosesLobbyWidget::HandlePlayerStateChanged_UI);
	LobbyLPS->OnLobbyRoomStateChanged().AddUObject(this, &UMosesLobbyWidget::HandleRoomStateChanged_UI);
}

void UMosesLobbyWidget::UnbindSubsystemEvents()
{
	if (!LobbyLPS)
	{
		return;
	}

	LobbyLPS->OnLobbyPlayerStateChanged().RemoveAll(this);
	LobbyLPS->OnLobbyRoomStateChanged().RemoveAll(this);
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
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
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
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	const bool bInRoom = PS->GetRoomId().IsValid() || bPendingEnterRoom_UIOnly;
	const bool bIsHost = PS->IsRoomHost();

	if (Button_StartGame)
	{
		Button_StartGame->SetVisibility((bInRoom && bIsHost) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CheckBox_Ready)
	{
		const bool bShowReady = (bInRoom && !bIsHost);
		CheckBox_Ready->SetVisibility(bShowReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		CheckBox_Ready->SetIsEnabled(bInRoom && !bIsHost && !bPendingEnterRoom_UIOnly);
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->SetVisibility(bInRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_LeaveRoom->SetIsEnabled(bInRoom && !bPendingEnterRoom_UIOnly);
	}
}

// ---------------------------
// UI Refresh Entry (Subsystem에서 호출)
// ---------------------------

void UMosesLobbyWidget::HandleRoomStateChanged_UI()
{
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid());

	if (!bInRoom && !bPendingEnterRoom_UIOnly)
	{
		RefreshRoomListFromGameState();
	}

	RefreshRightPanelControlsByRole();
	UpdateStartButton();
}

void UMosesLobbyWidget::HandlePlayerStateChanged_UI()
{
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	if (PS && PS->GetRoomId().IsValid() && bPendingEnterRoom_UIOnly)
	{
		EndPendingEnterRoom_UIOnly();
		return;
	}

	RefreshPanelsByPlayerState();
	RefreshRightPanelControlsByRole();
	UpdateStartButton();
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
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
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

	return (Room->MemberPids.Num() == Room->MaxPlayers) && Room->IsAllReady();
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
	APlayerController* PC = GetOwningPlayer();
	return Cast<AMosesPlayerController>(PC);
}

// ---------------------------
// Pending Enter Room (UI Only)
// ---------------------------

void UMosesLobbyWidget::BeginPendingEnterRoom_UIOnly()
{
	bPendingEnterRoom_UIOnly = true;

	RefreshPanelsByPlayerState();
	RefreshRightPanelControlsByRole();
	UpdateStartButton();

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

	RefreshPanelsByPlayerState();
	RefreshRightPanelControlsByRole();
	UpdateStartButton();
}

void UMosesLobbyWidget::OnPendingEnterRoomTimeout_UIOnly()
{
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid());

	if (!bInRoom)
	{
		EndPendingEnterRoom_UIOnly();
	}
}
