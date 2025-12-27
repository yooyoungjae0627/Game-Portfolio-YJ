// MosesPlayerController.cpp

#include "MosesPlayerController.h"

#include "UE5_Multi_Shooter/GameMode/MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/GameMode/MosesMatchGameMode.h"
#include "UE5_Multi_Shooter/GameMode/MosesLobbyGameState.h"

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

// 로비 RPC 파라미터 방어용(프로젝트 정책 값)
static constexpr int32 Lobby_MinRoomMaxPlayers = 2;
static constexpr int32 Lobby_MaxRoomMaxPlayers = 4;

AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();

	// 주의:
	// 생성자에서 bAutoManageActiveCameraTarget=false를 "영구"로 박아두면
	// 매치 상태에서도 Pawn 카메라가 자동으로 안 잡힐 수 있다.
	// -> 로비 컨텍스트에서만 BeginPlay에서 끄는 방식으로 처리한다.
}

/*
	IsLobbyContext
	- "로비 컨텍스트"인지 판단하는 헬퍼
	- 단일 맵(L_Match)에서 로비/매치를 같이 쓰더라도,
	  로비 단계에서만 UI/카메라를 강제하기 위해 필요하다.
	- 현재 구현: LobbyGameState가 존재하면 로비 컨텍스트로 간주
	  (너가 L_Match로 통일해도 GameState를 MosesLobbyGameState로 쓰거나,
	   Phase 시스템을 따로 만들기 전까진 이 방식이 제일 단순/안전)
*/
bool AMosesPlayerController::IsLobbyContext() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetGameState<AMosesLobbyGameState>() != nullptr);
}

/**
 * (Server RPC)
 * - "클라 입력"을 서버에서 처리하기 위한 진입점
 * - 실제 방 데이터 변경은 LobbyGameState에서 수행(복제)
 */
void AMosesPlayerController::Server_CreateRoom_Implementation(int32 MaxPlayers)
{
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

	// 입력값 방어(실수/악의적 호출 대비)
	const int32 SafeMaxPlayers = FMath::Clamp(MaxPlayers, Lobby_MinRoomMaxPlayers, Lobby_MaxRoomMaxPlayers);

	const FGuid NewRoomId = LGS->Server_CreateRoom(PS, SafeMaxPlayers);
	if (!NewRoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom FAIL Max=%d PC=%s"),
			SafeMaxPlayers, *GetNameSafe(this));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom OK Room=%s Max=%d PC=%s"),
		*NewRoomId.ToString(), SafeMaxPlayers, *GetNameSafe(this));
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

	if (bOk)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] JoinRoom OK"));
	}
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

/** (Server RPC) Start 버튼 → GameMode 검증 → Travel */
void AMosesPlayerController::Server_RequestStartGame_Implementation()
{
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

void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 디버그/로그용: PlayerState 찍기
	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->DOD_PS_Log(this, TEXT("PC:BeginPlay"));
	}

	// 1) 로컬 컨트롤러만 UI/카메라 세팅 (서버 전용 PC / 다른 플레이어 PC는 건드리면 안됨)
	if (!IsLocalController())
	{
		return;
	}

	// 2) "로비에서만" 로비 UI + 프리뷰 카메라 적용
	//    (단일 맵에서도 로비/매치 공존 가능하므로, 로비 컨텍스트 체크는 반드시 필요)
	if (!IsLobbyContext())
	{
		return;
	}

	// 2-1) 로비에서는 ViewTarget이 Pawn으로 자동 복구되는 걸 막기 위해 AutoManage를 끈다.
	//      (Possess/Experience/CameraManager가 ViewTarget을 덮어쓰는 케이스 방지)
	bAutoManageActiveCameraTarget = false;

	// 3) 로비 UI 띄우기 (LocalPlayerSubsystem)
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

	// 4) 로비 프리뷰 카메라 적용
	//   Experience/CameraManager/Possess 흐름에서 ViewTarget이 한 번 덮어쓰이는 경우가 있어서:
	//   - 즉시 1번
	//   - 약간 딜레이 후 1번(재적용)
	ApplyLobbyPreviewCamera();

	GetWorldTimerManager().SetTimer(
		LobbyPreviewCameraTimerHandle,                // 멤버 핸들(재사용 가능)
		this,
		&AMosesPlayerController::ApplyLobbyPreviewCamera,
		LobbyPreviewCameraDelay,                      // 예: 0.2f
		false                                         // 반복 X, 한 번만
	);
}

void AMosesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		PS->DOD_PS_Log(this, TEXT("PC:OnPossess"));
	}

	// 중요:
	// Possess 직후 엔진이 ViewTarget을 Pawn으로 다시 잡는 경우가 많다.
	// 그래서 "로비 컨텍스트"면 여기서도 프리뷰 카메라를 한 번 더 강제한다.
	if (IsLocalController() && IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
	}
}

// --------------------
// Existing Server RPCs
// --------------------

/** (Server RPC) Ready 토글 → PlayerState가 Source of Truth → RoomList(ReadyMap) 동기화 */
void AMosesPlayerController::Server_SetReady_Implementation(bool bInReady)
{
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

/** (Server RPC) 캐릭터 선택 → PlayerState에 저장(복제) */
void AMosesPlayerController::Server_SetSelectedCharacterId_Implementation(int32 InId)
{
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

	if (HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
		DoServerTravelToMatch();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] SEND TO SERVER (Client)"));
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
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
		DoServerTravelToLobby();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] SEND TO SERVER (Client)"));
	Server_TravelToLobby();
}

void AMosesPlayerController::Server_TravelToMatch_Implementation()
{
	if (!HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoAuthority)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
	DoServerTravelToMatch();
}

void AMosesPlayerController::Server_TravelToLobby_Implementation()
{
	if (!HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoAuthority)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
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

	if (AMosesMatchGameMode* MatchGM = Cast<AMosesMatchGameMode>(GM))
	{
		MatchGM->TravelToLobby();
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NotMatchGM) GM=%s"), *GetNameSafe(GM));
}

void AMosesPlayerController::ApplyLobbyPreviewCamera()
{
	if (!IsLocalController())
		return;

	if (!IsLobbyContext())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ACameraActor::StaticClass(), Found);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyCam] Found CameraActors=%d  TagNeed=%s"),
		Found.Num(), *LobbyPreviewCameraTag.ToString());

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
		UE_LOG(LogMosesSpawn, Error, TEXT("[LobbyCam] NOT FOUND. TagNeed=%s"), *LobbyPreviewCameraTag.ToString());
		return;
	}

	SetViewTargetWithBlend(TargetCam, 0.0f);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyCam] SET ViewTarget=%s Now=%s"),
		*GetNameSafe(TargetCam), *GetNameSafe(GetViewTarget()));
}
