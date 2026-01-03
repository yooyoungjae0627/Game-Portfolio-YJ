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

	CacheLocalSubsystem();
	BindLobbyButtons();
	BindListViewEvents();

	HandlePlayerStateChanged_UI();
	HandleRoomStateChanged_UI();
	UpdateStartButton();
}

void UMosesLobbyWidget::NativeDestruct()
{
	Super::NativeDestruct();

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

	CloseCreateRoomPopup();
}

// ---------------------------
// Setup / Bind
// ---------------------------

void UMosesLobbyWidget::CacheLocalSubsystem()
{
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		LobbyLPS = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>();
	}
}

void UMosesLobbyWidget::BindLobbyButtons()
{
	// 개발자 주석:
	// - 바인딩 실패는 대부분 WBP에서 IsVariable / 이름 불일치
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
		// 개발자 주석:
		// - ListView는 UObject 아이템을 받는 것이 정석
		// - GameState의 FastArray(UStruct)를 UI에 직접 꽂지 않는다.
		UMSLobbyRoomListItemData* Item = NewObject<UMSLobbyRoomListItemData>(this);
		Item->Init(Room.RoomId, Room.MaxPlayers, Room.MemberPids.Num());
		RoomListView->AddItem(Item);
	}
}

void UMosesLobbyWidget::RefreshPanelsByPlayerState()
{
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid());

	if (LeftPanel)
	{
		LeftPanel->SetVisibility(bInRoom ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}

	if (RightPanel)
	{
		RightPanel->SetVisibility(bInRoom ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void UMosesLobbyWidget::RefreshRightPanelControlsByRole()
{
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	const bool bInRoom = PS->GetRoomId().IsValid();
	const bool bIsHost = PS->IsRoomHost();

	// Host: Start만, Guest: Ready/Leave만
	if (Button_StartGame)
	{
		Button_StartGame->SetVisibility((bInRoom && bIsHost) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CheckBox_Ready)
	{
		CheckBox_Ready->SetVisibility((bInRoom && !bIsHost) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		CheckBox_Ready->SetIsEnabled(bInRoom && !bIsHost);
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->SetVisibility(bInRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_LeaveRoom->SetIsEnabled(bInRoom && !bIsHost);
	}
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

		// 개발자 주석:
		// - PopupWidget은 자기 책임으로 RPC 하지 않는다.
		// - LobbyWidget이 Confirm/Cancel을 받아서 PC RPC를 호출한다.
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

	// ✅ 중앙 고정
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

	// 개발자 주석:
	// - 재사용하고 싶으면 nullptr로 안 지워도 됨
	// - 지금은 단순하게 닫을 때 제거만 한다.
}

void UMosesLobbyWidget::HandleCreateRoomPopupConfirm(const FString& RoomTitle, int32 MaxPlayers)
{
	// 개발자 주석:
	// - RoomTitle은 Phase0에서는 서버 저장 안 하더라도 일단 받아두면 됨
	// - 서버는 MaxPlayers로 방 생성, Title은 나중에 RoomItem에 포함시키면 됨
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	PC->Server_CreateRoom(MaxPlayers);

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

	// 개발자 주석:
	// - AddToViewport로 들어간 위젯이 CanvasSlot을 가지는 경우에만 중앙 강제 가능
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

	Button_StartGame->SetIsEnabled(CanStartGame_UIOnly());
}

// ---------------------------
// UI Event handlers
// ---------------------------

void UMosesLobbyWidget::OnClicked_CreateRoom()
{
	// ✅ 기존 "바로 서버 생성" 대신: "팝업 띄우기"
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
	AMosesPlayerController* PC = GetMosesPC();
	if (!PC)
	{
		return;
	}

	PC->Server_SetReady(bIsChecked);
	UpdateStartButton();
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
// UI Refresh Entry (Subsystem에서 호출)
// ---------------------------
void UMosesLobbyWidget::HandleRoomStateChanged_UI()
{
	// 개발자 주석:
	// - 방 목록이 바뀌면: 리스트뷰 갱신 + 오른쪽 패널 정책 재계산 + Start 재계산
	RefreshRoomListFromGameState();
	RefreshRightPanelControlsByRole();
	UpdateStartButton();
}

void UMosesLobbyWidget::HandlePlayerStateChanged_UI()
{
	// 개발자 주석:
	// - 내 상태(Host/Ready/RoomId)가 바뀌면: 패널 토글 + 오른쪽 패널 정책 + Start 재계산
	RefreshPanelsByPlayerState();
	RefreshRightPanelControlsByRole();
	UpdateStartButton();
}