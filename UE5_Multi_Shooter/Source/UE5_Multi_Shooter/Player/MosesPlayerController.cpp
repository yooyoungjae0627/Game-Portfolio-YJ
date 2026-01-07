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

// --------------------------------------------------
// AMosesPlayerController (Ctor)
// --------------------------------------------------

AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 프로젝트 카메라 매니저 사용
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();

	/**
	 * 주의:
	 * - bAutoManageActiveCameraTarget 을 여기서 영구로 끄면 "매치에서 Pawn 카메라 자동 복구"가 망가질 수 있다.
	 * - 로비에서만 끄고, 로비가 아니면 반드시 원복한다. (BeginPlay/RestoreNonLobbyDefaults)
	 */
}

// --------------------------------------------------
// Dev Exec Helpers
// --------------------------------------------------

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

// --------------------------------------------------
// Client → Server RPC (Lobby)
// --------------------------------------------------
void AMosesPlayerController::Server_CreateRoom_Implementation(const FString& RoomTitle, int32 MaxPlayers)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("CREATE ROOM RPC CALLED"));

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_CreateRoom"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_CreateRoom"));
	if (!LGS || !PS)
	{
		return;
	}

	const int32 SafeMaxPlayers = FMath::Clamp(MaxPlayers, Lobby_MinRoomMaxPlayers, Lobby_MaxRoomMaxPlayers);

	// 개발자 주석:
	// - 서버에서도 한 번 더 Trim/길이 제한을 건다(최종 방어선).
	// - 빈 제목이면 디폴트로 치환(UX 안전망).
	FString SafeTitle = RoomTitle.TrimStartAndEnd().Left(24);
	if (SafeTitle.IsEmpty())
	{
		SafeTitle = TEXT("방 제목 없음");
	}

	const FGuid NewRoomId = LGS->Server_CreateRoom(PS, SafeTitle, SafeMaxPlayers);

	if (!NewRoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom FAIL Title=%s Max=%d PC=%s"),
			*SafeTitle, SafeMaxPlayers, *GetNameSafe(this));
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] CreateRoom OK Room=%s Title=%s Max=%d PC=%s"),
		*NewRoomId.ToString(), *SafeTitle, SafeMaxPlayers, *GetNameSafe(this));
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

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_JoinRoom"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_JoinRoom"));
	if (!LGS || !PS)
	{
		return;
	}

	EMosesRoomJoinResult Result = EMosesRoomJoinResult::Ok;
	const bool bOk = LGS->Server_JoinRoomWithResult(PS, RoomId, Result);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] JoinRoom %s Room=%s Result=%d PC=%s"),
		bOk ? TEXT("OK") : TEXT("FAIL"),
		*RoomId.ToString(),
		(int32)Result,
		*GetNameSafe(this));

	// ✅ Join 성공/실패를 해당 클라에게 즉시 통보
	Client_JoinRoomResult(Result, RoomId);
}

void AMosesPlayerController::Client_JoinRoomResult_Implementation(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	// 개발자 주석:
	// - UI 직접 조작 금지(네 원칙 유지)
	// - LocalPlayerSubsystem으로 결과만 전달한다.

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	if (UMosesLobbyLocalPlayerSubsystem* LobbySubsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
	{
		LobbySubsys->NotifyJoinRoomResult(Result, RoomId);
	}
}

void AMosesPlayerController::Server_LeaveRoom_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_LeaveRoom"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_LeaveRoom"));
	if (!LGS || !PS)
	{
		return;
	}

	LGS->Server_LeaveRoom(PS);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] LeaveRoom OK PC=%s"), *GetNameSafe(this));
}

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

void AMosesPlayerController::Server_SetReady_Implementation(bool bInReady)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] SetReady REJECT (NoPS)"));
		return;
	}

	// ✅ NEW: 호스트는 Ready가 없다 (서버에서도 강제)
	if (PS->IsRoomHost())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] SetReady REJECT (Host cannot Ready) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	// ✅ NEW: 방 밖이면 Ready 불가 (서버에서도 강제)
	if (!PS->GetRoomId().IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] SetReady REJECT (NotInRoom) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	// Ready 상태의 Source of Truth는 PlayerState
	PS->ServerSetReady(bInReady);
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterServer_SetReady"));

	// RoomList(MemberReady)로 동기화(복제 데이터 갱신)
	if (AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr)
	{
		LGS->Server_SyncReadyFromPlayerState(PS);
	}
}

void AMosesPlayerController::Server_SetSelectedCharacterId_Implementation(int32 InId)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SERVER] SetChar REJECT (NoPS)"));
		return;
	}

	PS->ServerSetSelectedCharacterId(InId);
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterServer_SetChar"));
}

// --------------------------------------------------
// Client ↔ Server (Login)
// --------------------------------------------------

void AMosesPlayerController::Server_RequestLogin_Implementation(const FString& InId, const FString& InPw)
{
	if (!HasAuthority())
	{
		return;
	}

	// PHASE0 정책: DB/보안 없음. "빈 값이면 실패" 정도만 검증.
	const FString Id = InId.TrimStartAndEnd();
	const FString Pw = InPw.TrimStartAndEnd();
	const bool bOk = (!Id.IsEmpty() && !Pw.IsEmpty());

	if (AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>())
	{
		// 서버 승인 기록(단일진실)
		PS->ServerSetLoggedIn(bOk);

		// DebugName도 이때 넣어주면 로그 가독성 상승
		if (bOk)
		{
			PS->DebugName = Id;
		}
	}

	Client_LoginResult(bOk, bOk ? TEXT("Login OK") : TEXT("Login FAIL (empty id/pw)"));
}

void AMosesPlayerController::Client_LoginResult_Implementation(bool bOk, const FString& Message)
{
	/**
	 * 원칙:
	 * - PlayerController는 "네트워크 이벤트 수신"까지.
	 * - 실제 UI 전환/위젯 갱신은 LocalPlayerSubsystem이 책임진다.
	 */
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	// TODO(너의 Flow Subsystem으로 연결):
	// - 로그인 성공이면 캐릭터 선택 UI로 전환
	// - 실패면 팝업/토스트 출력
	//
	// 예시:
	// if (UMosesFlowLocalPlayerSubsystem* Flow = LP->GetSubsystem<UMosesFlowLocalPlayerSubsystem>())
	// {
	//     Flow->NotifyLoginResult(bOk, Message);
	// }
}

// --------------------------------------------------
// Room Chat
// --------------------------------------------------

void AMosesPlayerController::Server_SendRoomChat_Implementation(const FString& Text)
{
	if (!HasAuthority())
	{
		return;
	}

	const FString Clean = Text.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!LGS || !PS)
	{
		return;
	}

	/**
	 * 권장 구조:
	 * - "중계/권한/룸 멤버 필터링"은 LobbyGameState가 단일 진실이어야 한다.
	 * - PlayerController는 '보낸다'는 요청만 전달.
	 */
	 // LGS->Server_BroadcastRoomChat(PS, Clean);
}

void AMosesPlayerController::Client_ReceiveRoomChat_Implementation(const FString& FromName, const FString& Text)
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	// TODO(Flow/Lobby Subsystem 쪽으로 위임)
	// if (UMosesFlowLocalPlayerSubsystem* Flow = LP->GetSubsystem<UMosesFlowLocalPlayerSubsystem>())
	// {
	//     Flow->NotifyRoomChatReceived(FromName, Text);
	// }
}

void AMosesPlayerController::Server_RequestSelectCharacter_Implementation(const FName CharacterId)
{
	// ✅ 서버 심판은 GameMode

	// 서버 RPC이므로 여기서는 서버에서만 실행되는 게 정상.
	UE_LOG(LogMosesSpawn, Log, TEXT("[CharSel][SV][PC] RPC Received CharacterId=%s PC=%s"),
		*CharacterId.ToString(), *GetNameSafe(this));

	AMosesLobbyGameMode* LobbyGM = Cast<AMosesLobbyGameMode>(UGameplayStatics::GetGameMode(this));
	if (!LobbyGM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][PC] REJECT (Not LobbyGM)"));
		return;
	}

	LobbyGM->HandleSelectCharacterRequest(this, CharacterId);
}

// --------------------------------------------------
// Lifecycle (Local UI / Camera)
// --------------------------------------------------

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

	/**
	 * SeamlessTravel 핵심 함정:
	 * - PlayerController가 유지되는 설정(bUseSeamlessTravel=true)에서는
	 *   로비에서 세팅한 "로컬 플래그/타이머"가 다음 맵에도 남아있다.
	 *
	 * 그래서 BeginPlay에서:
	 * - 로비면: 로비 정책 적용
	 * - 로비가 아니면: 로비 때 꺼둔 것들을 반드시 원복
	 */
	if (!IsLobbyContext())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly(); 
		return;
	}

	// 로비 정책 적용(로컬 전용)
	bAutoManageActiveCameraTarget = false;
	ActivateLobbyUI_LocalOnly();

	// Experience/Possess/CameraManager 흐름에서 ViewTarget이 덮이는 케이스가 있어
	// 즉시 1회 + 지연 1회 재적용(정책값 LobbyPreviewCameraDelay)
	ApplyLobbyPreviewCamera();
	ApplyLobbyInputMode_LocalOnly(); 

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
		ApplyLobbyInputMode_LocalOnly();
	}
}

void AMosesPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* LPS = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			LPS->NotifyPlayerStateChanged(); // 내부에서 Refresh까지 호출하고 있으니 OK
		}
	}
}

// --------------------------------------------------
// Travel Guard (Server RPC → Server Only DoXXX)
// --------------------------------------------------

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

void AMosesPlayerController::Server_DialogueAdvanceLine_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesLobbyGameMode>() : nullptr;
	if (!LobbyGM)
	{
		return;
	}

	LobbyGM->HandleDialogueAdvanceLineRequest(this);
}

void AMosesPlayerController::Server_DialogueSetFlowState_Implementation(EDialogueFlowState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesLobbyGameMode>() : nullptr;
	if (!LobbyGM)
	{
		return;
	}

	LobbyGM->HandleDialogueSetFlowStateRequest(this, NewState);
}

void AMosesPlayerController::Server_RequestEnterLobbyDialogue_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_RequestEnterLobbyDialogue"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_RequestEnterLobbyDialogue"));
	if (!LGS || !PS)
	{
		return;
	}

	// =========================================================
	// Gate Policy (권장)
	// - 로그인 필요
	// - 방 소속 필요
	// - 호스트만 Phase 전환 가능
	// =========================================================
	if (!PS->IsLoggedIn())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Gate][SV] Enter REJECT NotLoggedIn PC=%s"), *GetNameSafe(this));
		return;
	}

	if (!PS->GetRoomId().IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Gate][SV] Enter REJECT NotInRoom PC=%s"), *GetNameSafe(this));
		return;
	}

	if (!PS->IsRoomHost())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Gate][SV] Enter REJECT NotHost PC=%s"), *GetNameSafe(this));
		return;
	}

	if (LGS->GetGamePhase() == EGamePhase::LobbyDialogue)
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] Enter SKIP (AlreadyLobbyDialogue)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] Enter ACCEPT PC=%s"), *GetNameSafe(this));

	LGS->ServerEnterLobbyDialogue();
}

void AMosesPlayerController::Server_RequestExitLobbyDialogue_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_RequestExitLobbyDialogue"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_RequestExitLobbyDialogue"));
	if (!LGS || !PS)
	{
		return;
	}

	// 정책(권장): 호스트만 종료 가능
	if (!PS->GetRoomId().IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Gate][SV] Exit REJECT NotInRoom PC=%s"), *GetNameSafe(this));
		return;
	}

	if (!PS->IsRoomHost())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Gate][SV] Exit REJECT NotHost PC=%s"), *GetNameSafe(this));
		return;
	}


	if (LGS->GetGamePhase() == EGamePhase::Lobby)
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] Exit SKIP (AlreadyLobby)"));
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] Exit ACCEPT PC=%s"), *GetNameSafe(this));
	LGS->ServerExitLobbyDialogue();
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

// --------------------------------------------------
// Lobby Context / Camera
// --------------------------------------------------

bool AMosesPlayerController::IsLobbyContext() const
{
	/**
	 * 로비 UI/카메라 강제 적용을 "로비 단계"에만 제한하기 위한 게이트.
	 * 정책이 바뀌면(Phase 기반 등) 여기만 수정해도 전체 로비 적용 조건이 바뀐다.
	 */
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetGameState<AMosesLobbyGameState>() != nullptr);
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

void AMosesPlayerController::ActivateLobbyUI_LocalOnly()
{
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
}

void AMosesPlayerController::RestoreNonLobbyDefaults_LocalOnly()
{
	/**
	 * SeamlessTravel에서 PlayerController가 유지되면:
	 * - 로비에서 꺼둔 bAutoManageActiveCameraTarget=false 가
	 *   매치까지 따라와서 "Pawn 카메라 자동 복구"가 죽을 수 있다.
	 *
	 * 그래서 로비가 아닌 맵에서는 무조건 원복해 준다.
	 */
	bAutoManageActiveCameraTarget = true;

	// 로비에서 걸어둔 "재적용 타이머"도 다음 맵에서 의미 없으니 제거
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(LobbyPreviewCameraTimerHandle);
	}
}

// --------------------------------------------------
// Shared getters (null/log guard)
// --------------------------------------------------

AMosesLobbyGameState* AMosesPlayerController::GetLobbyGameStateChecked_Log(const TCHAR* Caller) const
{
	UWorld* World = GetWorld();
	AMosesLobbyGameState* LGS = World ? World->GetGameState<AMosesLobbyGameState>() : nullptr;

	if (!LGS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[%s] REJECT (NoLobbyGameState) PC=%s"), Caller, *GetNameSafe(this));
	}

	return LGS;
}

AMosesPlayerState* AMosesPlayerController::GetMosesPlayerStateChecked_Log(const TCHAR* Caller) const
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[%s] REJECT (NoPlayerState) PC=%s"), Caller, *GetNameSafe(this));
	}

	return PS;
}

void AMosesPlayerController::ApplyLobbyInputMode_LocalOnly()
{
	// 개발자 주석:
	// - 로비에서는 마우스 커서가 항상 보여야 하고 UI 클릭이 가능해야 한다.
	// - GameOnly 입력 모드가 되면 커서가 사라지거나 UI 입력이 씹힐 수 있다.
	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	// 포커스 위젯은 Subsystem이 로비 위젯을 들고 있으니, Phase0에서는 비워도 OK.
	// (원하면 LobbyWidget을 꺼내서 SetWidgetToFocus까지 걸 수 있음)
	SetInputMode(Mode);
}

void AMosesPlayerController::RestoreNonLobbyInputMode_LocalOnly()
{
	// 개발자 주석:
	// - 로비에서 나가면 게임 입력으로 복구해야 한다.
	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
}
