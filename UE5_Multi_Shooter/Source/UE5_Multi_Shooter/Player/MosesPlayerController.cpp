#include "MosesPlayerController.h"

#include "UE5_Multi_Shooter/GameMode/GameMode/MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesMatchGameMode.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "TimerManager.h"

// 로비 RPC 파라미터 정책(서버에서 Clamp)
static constexpr int32 Lobby_MinRoomMaxPlayers = 2;
static constexpr int32 Lobby_MaxRoomMaxPlayers = 4;

AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 프로젝트 카메라 매니저 사용
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();

	// ⚠ bAutoManageActiveCameraTarget은 로비 컨텍스트에서만 끈다.
	// 생성자에서 영구로 꺼두면 매치에서 Pawn 카메라 자동 복구가 안 될 수 있음.
}

bool AMosesPlayerController::IsLobbyContext() const
{
	// 로비 UI/카메라 강제 적용을 "로비 단계"에만 제한하기 위한 게이트.
	// 정책이 바뀌면(Phase 기반 등) 여기만 수정해도 전체 로비 적용 조건이 바뀐다.
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetGameState<AMosesLobbyGameState>() != nullptr);
}

// ---------------------------
// Lobby RPCs (Server)
// ---------------------------

void AMosesPlayerController::Server_CreateRoom_Implementation(int32 MaxPlayers)
{
	// RPC는 서버에서만 유효 (Client 호출이 들어와도 여기서 최종 방어)
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();

	if (!LGS || !PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom REJECT (NoGS/NoPS)"));
		return;
	}

	// 입력값 방어: 실수/악의적 호출 대비
	const int32 SafeMaxPlayers = FMath::Clamp(MaxPlayers, Lobby_MinRoomMaxPlayers, Lobby_MaxRoomMaxPlayers);

	// 룸 생성은 LobbyGameState가 Source of Truth
	const FGuid NewRoomId = LGS->Server_CreateRoom(PS, SafeMaxPlayers);
	if (!NewRoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom FAIL Max=%d PC=%s"),
			SafeMaxPlayers, *GetNameSafe(this));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom OK Room=%s Max=%d PC=%s"),
		*NewRoomId.ToString(), SafeMaxPlayers, *GetNameSafe(this));

	// RoomId를 클라 UI에 자동 입력하고 싶으면:
	// - 여기서 ClientRPC로 Subsystem.NotifyRoomCreated(NewRoomId) 호출하는 구조로 확장 가능
}

void AMosesPlayerController::Server_JoinRoom_Implementation(const FGuid& RoomId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!RoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] JoinRoom REJECT (InvalidRoomId) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();

	if (!LGS || !PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] JoinRoom REJECT (NoGS/NoPS)"));
		return;
	}

	const bool bOk = LGS->Server_JoinRoom(PS, RoomId);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] JoinRoom %s Room=%s PC=%s"),
		bOk ? TEXT("OK") : TEXT("FAIL"), *RoomId.ToString(), *GetNameSafe(this));
}

void AMosesPlayerController::Server_LeaveRoom_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();

	if (!LGS || !PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] LeaveRoom REJECT (NoGS/NoPS)"));
		return;
	}

	LGS->Server_LeaveRoom(PS);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] LeaveRoom OK PC=%s"), *GetNameSafe(this));
}

void AMosesPlayerController::Server_RequestStartGame_Implementation()
{
	// StartGame은 서버 GM에서 최종 검증 후 Travel
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesLobbyGameMode>() : nullptr;
	if (!LobbyGM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] StartGame REJECT (NotLobbyGM)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] StartGame REQ PC=%s"), *GetNameSafe(this));
	LobbyGM->HandleStartGameRequest(this);
}

// ---------------------------
// Lifecycle (Local UI / Camera)
// ---------------------------

void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// DOD 로그: PS 유지/재생성/값 확인용
	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->DOD_PS_Log(this, TEXT("PC:BeginPlay"));
	}

	// 로컬 컨트롤러만 UI/카메라 변경 가능
	if (!IsLocalController())
	{
		return;
	}

	// 로비 컨텍스트에서만 로비 UI/카메라 적용
	if (!IsLobbyContext())
	{
		return;
	}

	// 로비에서는 ViewTarget이 Pawn으로 자동 복구되는 것을 막기 위해 AutoManage를 끈다.
	// (Possess/Experience/CameraManager가 ViewTarget을 덮는 케이스 방지)
	bAutoManageActiveCameraTarget = false;

	// 로비 UI 띄우기(LocalPlayerSubsystem)
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyUI] PC BeginPlay: No LocalPlayer"));
		return;
	}

	if (UMosesLobbyLocalPlayerSubsystem* LobbySubsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
	{
		LobbySubsys->ActivateLobbyUI();
	}
	else
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[LobbyUI] PC BeginPlay: No MosesLobbyLocalPlayerSubsystem"));
	}

	// Experience/Possess/CameraManager 흐름에서 ViewTarget이 덮이는 케이스가 있어
	// 즉시 1회 + 지연 1회 재적용(정책값 LobbyPreviewCameraDelay)
	ApplyLobbyPreviewCamera();

	GetWorldTimerManager().SetTimer(
		LobbyPreviewCameraTimerHandle,
		this,
		&AMosesPlayerController::ApplyLobbyPreviewCamera,
		LobbyPreviewCameraDelay,
		false
	);
}

void AMosesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->DOD_PS_Log(this, TEXT("PC:OnPossess"));
	}

	// Possess 직후 ViewTarget이 Pawn으로 덮일 수 있어 로비면 재적용한다.
	if (IsLocalController() && IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
	}
}

// --------------------
// Existing Server RPCs
// --------------------

void AMosesPlayerController::Server_SetReady_Implementation(bool bInReady)
{
	// Ready 상태의 Source of Truth는 PlayerState
	// 이후 RoomList의 ReadyMap을 LGS에 동기화(복제 데이터 갱신)
	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->ServerSetReady(bInReady);
		PS->DOD_PS_Log(this, TEXT("Lobby:AfterServer_SetReady"));

		if (AMosesLobbyGameState* LGS = GetWorld()->GetGameState<AMosesLobbyGameState>())
		{
			LGS->Server_SyncReadyFromPlayerState(PS);
		}
	}
}

void AMosesPlayerController::Server_SetSelectedCharacterId_Implementation(int32 InId)
{
	// 캐릭터 선택은 PlayerState에 저장(복제)
	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->ServerSetSelectedCharacterId(InId);
		PS->DOD_PS_Log(this, TEXT("Lobby:AfterServer_SetChar"));
	}
}

// --------------------
// Travel (Exec -> Server RPC -> GM)
// --------------------

void AMosesPlayerController::TravelToMatch_Exec()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	// 서버면 즉시 실행, 클라면 서버 RPC로 우회
	if (HasAuthority())
	{
		DoServerTravelToMatch();
		return;
	}

	Server_TravelToMatch();
}

void AMosesPlayerController::TravelToLobby_Exec()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	if (HasAuthority())
	{
		DoServerTravelToLobby();
		return;
	}

	Server_TravelToLobby();
}

void AMosesPlayerController::Server_TravelToMatch_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	DoServerTravelToMatch();
}

void AMosesPlayerController::Server_TravelToLobby_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	DoServerTravelToLobby();
}

void AMosesPlayerController::DoServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoGameMode)"));
		return;
	}

	// 로비 GM일 때만 매치 이동 허용
	if (AMosesLobbyGameMode* LobbyGM = Cast<AMosesLobbyGameMode>(GM))
	{
		LobbyGM->TravelToMatch();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NotLobbyGM) GM=%s"), *GetNameSafe(GM));
}

void AMosesPlayerController::DoServerTravelToLobby()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoGameMode)"));
		return;
	}

	// 매치 GM일 때만 로비 이동 허용
	if (AMosesMatchGameMode* MatchGM = Cast<AMosesMatchGameMode>(GM))
	{
		MatchGM->TravelToLobby();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NotMatchGM) GM=%s"), *GetNameSafe(GM));
}

void AMosesPlayerController::ApplyLobbyPreviewCamera()
{
	// 로컬 + 로비 컨텍스트에서만 카메라 강제
	if (!IsLocalController() || !IsLobbyContext())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Tag가 붙은 CameraActor를 찾아 ViewTarget으로 설정한다.
	// (맵에 카메라가 없거나 Tag가 다르면 로비 UI는 떠도 화면이 이상해질 수 있음)
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ACameraActor::StaticClass(), Found);

	ACameraActor* TargetCam = nullptr;
	for (AActor* A : Found)
	{
		if (A && A->ActorHasTag(LobbyPreviewCameraTag))
		{
			TargetCam = Cast<ACameraActor>(A);
			break;
		}
	}

	if (!TargetCam)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[LobbyCam] NOT FOUND TagNeed=%s"), *LobbyPreviewCameraTag.ToString());
		return;
	}

	SetViewTargetWithBlend(TargetCam, 0.0f);
}
