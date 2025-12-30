#include "MosesLobbyWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"

void UMosesLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 바인딩 실패는 WBP 설정 문제(이름 불일치/IsVariable 미체크)일 가능성이 높다.
	// 실패 지점을 바로 찾을 수 있게 각 위젯마다 로그를 분리해 둔다.

	if (Button_CreateRoom)
	{
		Button_CreateRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_CreateRoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Bind FAIL: Button_CreateRoom"));
	}

	if (Button_JoinRoom)
	{
		Button_JoinRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_JoinRoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Bind FAIL: Button_JoinRoom"));
	}

	if (Button_LeaveRoom)
	{
		Button_LeaveRoom->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_LeaveRoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Bind FAIL: Button_LeaveRoom"));
	}

	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.AddDynamic(this, &UMosesLobbyWidget::OnClicked_StartGame);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Bind FAIL: Button_StartGame"));
	}

	if (CheckBox_Ready)
	{
		CheckBox_Ready->OnCheckStateChanged.AddDynamic(this, &UMosesLobbyWidget::OnReadyChanged);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Bind FAIL: CheckBox_Ready"));
	}

	if (!TextBox_RoomId)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Bind FAIL: TextBox_RoomId"));
	}

	// 초기 UI 상태 반영(로비 진입 직후, 혹은 Rep 데이터가 이미 있는 경우 대비)
	UpdateStartButton();
}

AMosesPlayerController* UMosesLobbyWidget::GetMosesPC() const
{
	// 위젯의 OwningPlayer는 LocalPlayer 기준으로 설정된다.
	// 서버 RPC 호출은 MosesPlayerController에서만 하므로 여기서 캐스팅한다.
	return Cast<AMosesPlayerController>(GetOwningPlayer());
}

void UMosesLobbyWidget::OnClicked_CreateRoom()
{
	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] CreateRoom FAIL: MosesPC cast"));
		return;
	}

	// 실제 룸 생성/정원 검증은 서버에서 결정한다.
	// DebugMaxPlayers는 테스트 편의를 위한 입력값.
	MPC->Server_CreateRoom(DebugMaxPlayers);
}

void UMosesLobbyWidget::OnClicked_LeaveRoom()
{
	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] LeaveRoom FAIL: MosesPC cast"));
		return;
	}

	MPC->Server_LeaveRoom();
}

void UMosesLobbyWidget::OnReadyChanged(bool bIsChecked)
{
	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Ready FAIL: MosesPC cast"));
		return;
	}

	// Ready 상태는 서버 authoritative로 저장/복제된다.
	MPC->Server_SetReady(bIsChecked);

	// 내 입력 직후 UI 반응(체감)용. 최종 상태는 Rep 갱신에서 다시 확정된다.
	UpdateStartButton();
}

void UMosesLobbyWidget::OnClicked_StartGame()
{
	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] StartGame FAIL: MosesPC cast"));
		return;
	}

	// 서버에서 Host/AllReady/Full 조건을 다시 검증하고 Travel 트리거한다.
	MPC->Server_RequestStartGame();
}

void UMosesLobbyWidget::OnClicked_JoinRoom()
{
	AMosesPlayerController* MPC = GetMosesPC();
	if (!MPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] JoinRoom FAIL: MosesPC cast"));
		return;
	}

	// 룸ID 입력값 파싱(유효하지 않으면 서버 RPC 호출 금지)
	FString RoomIdStr;
	if (TextBox_RoomId)
	{
		RoomIdStr = TextBox_RoomId->GetText().ToString().TrimStartAndEnd();
	}

	FGuid RoomId;
	if (!FGuid::Parse(RoomIdStr, RoomId) || !RoomId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] JoinRoom REJECT: Invalid GUID '%s'"), *RoomIdStr);
		return;
	}

	MPC->Server_JoinRoom(RoomId);
}

void UMosesLobbyWidget::SetRoomIdText(const FGuid& RoomId)
{
	// CreateRoom 성공 후 자동 입력(Host가 복사/붙여넣기 안 해도 됨)
	if (!TextBox_RoomId)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] SetRoomIdText FAIL: TextBox_RoomId bind"));
		return;
	}

	const FString Str = RoomId.ToString(EGuidFormats::DigitsWithHyphens);
	TextBox_RoomId->SetText(FText::FromString(Str));
}

bool UMosesLobbyWidget::CanStartGame() const
{
	// Start 가능 여부는 "클라 UI에서 계산"하지만,
	// 서버는 동일 조건을 다시 검증해야 한다(클라 조작 방지).
	const AMosesPlayerState* PS = GetOwningPlayerState<AMosesPlayerState>();
	if (!PS || !PS->bIsRoomHost)
	{
		return false;
	}

	const AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	if (!LGS)
	{
		return false;
	}

	const FMosesLobbyRoomItem* Room = LGS->FindRoom(PS->RoomId);
	if (!Room)
	{
		return false;
	}

	return (Room->MemberPids.Num() == Room->MaxPlayers) && Room->IsAllReady();
}

void UMosesLobbyWidget::UpdateStartButton()
{
	// UI가 아직 완전히 바인딩 되기 전 호출될 수 있으니 방어
	if (!Button_StartGame)
	{
		return;
	}

	const bool bEnable = CanStartGame();
	Button_StartGame->SetIsEnabled(bEnable);

	// 필요하면 로그 레벨 낮추는 것을 권장(Rep 갱신 때마다 찍히면 스팸)
	// UE_LOG(LogTemp, Log, TEXT("[LobbyUI] StartButton=%s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}
