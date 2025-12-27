// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesLobbyWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"

void UMosesLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼/체크박스 바인딩
	if (Button_CreateRoom)
	{
		Button_CreateRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CreateRoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Button_CreateRoom is null (BindWidget failed)"));
	}

	if (Button_JoinRoom)
	{
		Button_JoinRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_JoinRoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Button_JoinRoom is null (BindWidget failed)"));
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_LeaveRoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Button_LeaveRoom is null (BindWidget failed)"));
	}

	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_StartGame);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Button_StartGame is null (BindWidget failed)"));
	}

	if (CheckBox_Ready)
	{
		CheckBox_Ready->OnCheckStateChanged.AddDynamic(this, &UMosesLobbyWidget::OnReadyChanged);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] CheckBox_Ready is null (BindWidget failed)"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] UMosesLobbyWidget::NativeConstruct complete"));
}

AMosesPlayerController* UMosesLobbyWidget::GetMosesPC() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return Cast<AMosesPlayerController>(PC);
	}
	return nullptr;
}

void UMosesLobbyWidget::OnClicked_CreateRoom()
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] CreateRoom Clicked"));

	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] CreateRoom: Cast MosesPC failed"));
		return;
	}

	MPC->Server_CreateRoom(DebugMaxPlayers);
}

void UMosesLobbyWidget::OnClicked_LeaveRoom()
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] LeaveRoom Clicked"));

	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] LeaveRoom: Cast MosesPC failed"));
		return;
	}

	MPC->Server_LeaveRoom();
}

void UMosesLobbyWidget::OnReadyChanged(bool bIsChecked)
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] Ready Changed = %s"), bIsChecked ? TEXT("TRUE") : TEXT("FALSE"));

	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Ready: Cast MosesPC failed"));
		return;
	}

	MPC->Server_SetReady(bIsChecked);
}

void UMosesLobbyWidget::OnClicked_StartGame()
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] StartGame Clicked"));

	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] StartGame: Cast MosesPC failed"));
		return;
	}

	MPC->Server_RequestStartGame();
}

void UMosesLobbyWidget::OnClicked_JoinRoom()
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom Clicked"));

	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] JoinRoom: Cast MosesPC failed"));
		return;
	}

	FString RoomIdStr;
	if (TextBox_RoomId)
	{
		RoomIdStr = TextBox_RoomId->GetText().ToString().TrimStartAndEnd();
	}

	FGuid RoomId;
	if (!FGuid::Parse(RoomIdStr, RoomId) || !RoomId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom: Invalid GUID input: '%s'"), *RoomIdStr);
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom: Example GUID format: 01234567-89AB-CDEF-0123-456789ABCDEF"));
		return;
	}

	MPC->Server_JoinRoom(RoomId);  
}



