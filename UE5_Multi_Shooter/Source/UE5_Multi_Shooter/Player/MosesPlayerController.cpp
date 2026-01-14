#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"

#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesStartGameMode.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesMatchGameMode.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// --------------------------------------------------
// 로비 RPC 파라미터 정책(서버에서 Clamp)
// --------------------------------------------------
static constexpr int32 Lobby_MinRoomMaxPlayers = 2;
static constexpr int32 Lobby_MaxRoomMaxPlayers = 4;

// =========================================================
// AMosesPlayerController (Ctor)
// =========================================================

AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 프로젝트 카메라 매니저 사용
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();

	/**
	 * 주의:
	 * - bAutoManageActiveCameraTarget을 ctor에서 영구로 끄면 매치에서 Pawn 카메라 자동 복구가 깨질 수 있음.
	 * - 로비에서만 끄고, 로비가 아니면 반드시 원복한다. (BeginPlay / RestoreNonLobbyDefaults_LocalOnly)
	 */
}

// =========================================================
// Dev Exec Helpers
// =========================================================

void AMosesPlayerController::TravelToMatch_Exec()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return;
	}

	// 서버면 즉시 실행, 클라는 서버 RPC로 우회
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

// =========================================================
// Lifecycle (Local UI / Camera)
// =========================================================

void AMosesPlayerController::SetPendingLobbyNickname_Local(const FString& Nick)
{
	if (!IsLocalController())
	{
		return;
	}

	PendingLobbyNickname_Local = Nick.TrimStartAndEnd();

	if (PendingLobbyNickname_Local.IsEmpty())
	{
		bPendingLobbyNicknameSend_Local = false;
		return;
	}

	bPendingLobbyNicknameSend_Local = true;

	// 이미 조건이 만족되면 즉시 전송 시도
	TrySendPendingLobbyNickname_Local();
}

void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 컨트롤러만 UI/카메라 조작
	if (!IsLocalController())
	{
		return;
	}

	/**
	 * SeamlessTravel 함정:
	 * - PC가 유지되면 로비에서 설정한 플래그/타이머가 다음 맵까지 남을 수 있다.
	 * - 따라서 BeginPlay에서 "로비면 적용 / 아니면 원복"을 반드시 수행한다.
	 */
	if (!IsLobbyContext())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();
		return;
	}

	// 로비 정책 적용
	bAutoManageActiveCameraTarget = false;

	ActivateLobbyUI_LocalOnly();
	ApplyLobbyInputMode_LocalOnly();

	// ViewTarget은 Experience/OnPossess 등에서 덮일 수 있어 즉시 + 지연 재적용
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

	// Possess 직후 ViewTarget이 Pawn으로 덮일 수 있으니, 로비면 재적용
	if (IsLocalController() && IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly();
	}

	UE_LOG(LogTemp, Warning, TEXT("[TEST][Cam] PC=%s Pawn=%s ViewTarget=%s AutoManage=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InPawn),
		*GetNameSafe(GetViewTarget()),
		bAutoManageActiveCameraTarget ? 1 : 0);

}

void AMosesPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			Subsys->NotifyPlayerStateChanged();
		}
	}
}

// =========================================================
// Client → Server RPC (Lobby)
// =========================================================

void AMosesPlayerController::Server_CreateRoom_Implementation(const FString& RoomTitle, int32 MaxPlayers)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_CreateRoom"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_CreateRoom"));
	if (!LGS || !PS)
	{
		return;
	}

	PS->EnsurePersistentId_Server();

	const int32 SafeMaxPlayers = FMath::Clamp(MaxPlayers, Lobby_MinRoomMaxPlayers, Lobby_MaxRoomMaxPlayers);

	// 서버 최종 방어선: Trim + 길이 제한 + 빈 제목 보호
	FString SafeTitle = RoomTitle.TrimStartAndEnd().Left(24);
	if (SafeTitle.IsEmpty())
	{
		SafeTitle = TEXT("방 제목 없음");
	}

	const FGuid NewRoomId = LGS->Server_CreateRoom(PS, SafeTitle, SafeMaxPlayers);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyRPC][SV] CreateRoom %s Room=%s Title=%s Max=%d PC=%s"),
		NewRoomId.IsValid() ? TEXT("OK") : TEXT("FAIL"),
		*NewRoomId.ToString(),
		*SafeTitle,
		SafeMaxPlayers,
		*GetNameSafe(this));
}
void AMosesPlayerController::Server_JoinRoom_Implementation(const FGuid& RoomId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!RoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] JoinRoom REJECT (InvalidRoomId) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_JoinRoom"));
	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_JoinRoom"));
	if (!LGS || !PS)
	{
		return;
	}

	// =====================================================
	// 1) PID는 Join 이전에 반드시 보장
	// =====================================================
	PS->EnsurePersistentId_Server();

	// =====================================================
	// 2) ✅ NotLoggedIn 레이스 흡수 (가장 중요)
	// - Join이 Nick RPC보다 먼저 도착하면 서버에서 NotLoggedIn으로 튕김
	// - 정책: Join 시점에 LoggedIn이 아니면 "Guest"로 자동 로그인 확정 후 진행
	// =====================================================
	if (!PS->IsLoggedIn())
	{
		// 닉이 없으면 Guest 닉 생성
		FString AutoNick = PS->GetPlayerNickName().TrimStartAndEnd();
		if (AutoNick.IsEmpty())
		{
			const FString Pid4 = PS->GetPersistentId().ToString(EGuidFormats::Digits).Left(4);
			AutoNick = FString::Printf(TEXT("Guest_%s"), *Pid4);
		}

		PS->ServerSetPlayerNickName(AutoNick);
		PS->ServerSetLoggedIn(true);

		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[LobbyRPC][SV] AutoLoginBeforeJoin Nick=%s Pid=%s PC=%s"),
			*AutoNick,
			*PS->GetPersistentId().ToString(),
			*GetNameSafe(this));
	}

	// =====================================================
	// 3) Join 수행
	// =====================================================
	EMosesRoomJoinResult Result = EMosesRoomJoinResult::Ok;
	const bool bOk = LGS->Server_JoinRoomWithResult(PS, RoomId, Result);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyRPC][SV] JoinRoom %s Room=%s Result=%d PC=%s Pid=%s LoggedIn=%d"),
		bOk ? TEXT("OK") : TEXT("FAIL"),
		*RoomId.ToString(),
		(int32)Result,
		*GetNameSafe(this),
		*PS->GetPersistentId().ToString(),
		PS->IsLoggedIn() ? 1 : 0);

	// 결과를 해당 클라에게 즉시 통보
	Client_JoinRoomResult(Result, RoomId);
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

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyRPC][SV] LeaveRoom OK PC=%s"), *GetNameSafe(this));
}

void AMosesPlayerController::Server_SetReady_Implementation(bool bInReady)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_SetReady"));
	if (!PS)
	{
		return;
	}

	// 호스트는 Ready 불가 (서버 강제)
	if (PS->IsRoomHost())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] SetReady REJECT (Host cannot Ready) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	// 방 밖이면 Ready 불가 (서버 강제)
	if (!PS->GetRoomId().IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] SetReady REJECT (NotInRoom) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	// Ready 단일 진실은 PlayerState
	PS->ServerSetReady(bInReady);
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterServer_SetReady"));

	// RoomList/MemberReady 동기화 (복제 데이터 갱신)
	if (AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr)
	{
		LGS->Server_SyncReadyFromPlayerState(PS);
	}
}

void AMosesPlayerController::Server_RequestStartMatch_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesLobbyGameMode>() : nullptr;
	if (!LobbyGM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] StartMatch REJECT (NotLobbyGM) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	// 최종 판정(호스트 여부/인원/Ready 등)은 GM에서
	LobbyGM->HandleStartMatchRequest(GetPlayerState<AMosesPlayerState>());
}

void AMosesPlayerController::Server_SetLobbyNickname_Implementation(const FString& Nick)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_SetLobbyNickname"));
	if (!PS)
	{
		return;
	}

	// ✅ 핵심: 로그인 확정 전에 PID부터 보장
	PS->EnsurePersistentId_Server();

	const FString Clean = Nick.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return;
	}

	PS->ServerSetPlayerNickName(Clean);

	// ✅ 정책: LoggedIn=true 라면 PID는 반드시 유효해야 한다.
	PS->ServerSetLoggedIn(true);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[NickSV] PC=%s PS=%s Nick=%s Pid=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PS),
		*Clean,
		*PS->GetPersistentId().ToString());
}

void AMosesPlayerController::Server_SendLobbyChat_Implementation(const FString& Text)
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

	AMosesLobbyGameState* MosesLobbyGameState = GetLobbyGameStateChecked_Log(TEXT("Server_SendLobbyChat"));
	AMosesPlayerState* MosesPlayerState = GetMosesPlayerStateChecked_Log(TEXT("Server_SendLobbyChat"));
	if (!MosesLobbyGameState || !MosesPlayerState)
	{
		return;
	}

	// GameState가 채팅 단일 진실 (추가/복제/브로드캐스트 책임)
	MosesLobbyGameState->Server_AddChatMessage(MosesPlayerState, Clean);
}

// =========================================================
// JoinRoom Result (Server → Client)
// =========================================================

void AMosesPlayerController::Client_JoinRoomResult_Implementation(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	// UI 직접 조작 금지: Subsystem으로 위임
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

// =========================================================
// Dialogue Gate (Client → Server)
// =========================================================

void AMosesPlayerController::Server_RequestEnterLobbyDialogue_Implementation(ELobbyDialogueEntryType EntryType)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	const bool bInRoom = (PS && PS->GetRoomId().IsValid());

	// RulesView는 방 없어도 허용 / InRoomDialogue는 방 필요
	if (EntryType == ELobbyDialogueEntryType::InRoomDialogue && !bInRoom)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Gate][SV] EnterDialogue REJECT NotInRoom PC=%s"),
			*GetNameSafe(this));
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_RequestEnterLobbyDialogue"));
	if (!LGS)
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] EnterDialogue ACCEPT Type=%d InRoom=%d PC=%s"),
		(int32)EntryType, bInRoom ? 1 : 0, *GetNameSafe(this));

	// 실제 진입 처리/Phase 변경은 GameState가 단일 진실
	LGS->ServerEnterLobbyDialogue(/*필요하면 EntryType 전달 버전으로 확장*/);
}

void AMosesPlayerController::Server_RequestExitLobbyDialogue_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("Server_RequestExitLobbyDialogue"));
	if (!LGS)
	{
		return;
	}

	// LobbyDialogue 상태가 아니면 무시
	if (LGS->GetGamePhase() != EGamePhase::LobbyDialogue)
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] ExitDialogue SKIP (Not LobbyDialogue phase) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][Gate][SV] ExitDialogue ACCEPT PC=%s"),
		*GetNameSafe(this));

	LGS->ServerExitLobbyDialogue();
}

void AMosesPlayerController::Server_SubmitDialogueCommand_Implementation(EDialogueCommandType Type, uint16 ClientCommandSeq)
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

	// LobbyGM->HandleSubmitDialogueCommand(this, Type, ClientCommandSeq);
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

// =========================================================
// Travel Guard (Dev Exec → Server Only)
// =========================================================

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

void AMosesPlayerController::TrySendPendingLobbyNickname_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!bPendingLobbyNicknameSend_Local)
	{
		return;
	}

	// 커넥션 준비 전이면 대기 (Travel 직후 등)
	if (!GetNetConnection())
	{
		return;
	}

	// PS 준비 전이면 대기
	if (!PlayerState)
	{
		return;
	}

	if (PendingLobbyNickname_Local.IsEmpty())
	{
		bPendingLobbyNicknameSend_Local = false;
		return;
	}

	// ✅ PS 준비 완료 시점에서만 서버 RPC 전송
	Server_SetLobbyNickname(PendingLobbyNickname_Local);

	// 1회 전송 완료
	bPendingLobbyNicknameSend_Local = false;
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

// =========================================================
// Lobby Context / Camera / UI
// =========================================================

bool AMosesPlayerController::IsLobbyContext() const
{
	/**
	 * 로비 UI/카메라 오염 방지용 게이트
	 * - 정책이 바뀌면 여기만 수정하면 전체 로비 조건이 바뀜
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
	if (!IsLocalController() || !IsLobbyContext())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

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
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyUI] No LocalPlayer PC=%s"), *GetNameSafe(this));
		return;
	}

	if (UMosesLobbyLocalPlayerSubsystem* LobbySubsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
	{
		LobbySubsys->ActivateLobbyUI();
	}
	else
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[LobbyUI] No MosesLobbyLocalPlayerSubsystem PC=%s"), *GetNameSafe(this));
	}
}

void AMosesPlayerController::RestoreNonLobbyDefaults_LocalOnly()
{
	/**
	 * SeamlessTravel 대비:
	 * - 로비에서 끈 bAutoManageActiveCameraTarget이 매치까지 따라오면 Pawn 카메라 자동복구가 죽는다.
	 */
	bAutoManageActiveCameraTarget = true;

	// 로비에서 걸어둔 타이머 제거
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(LobbyPreviewCameraTimerHandle);
	}
}

void AMosesPlayerController::ApplyLobbyInputMode_LocalOnly()
{
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

	SetInputMode(Mode);
}

void AMosesPlayerController::RestoreNonLobbyInputMode_LocalOnly()
{
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

// =========================================================
// Shared getters (null/log guard)
// =========================================================

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

void AMosesPlayerController::Server_RequestEnterLobby_Implementation(const FString& Nickname)
{
	if (!HasAuthority())
	{
		return;
	}

	// ✅ 여기서 Nick 세팅 금지!
	// - PS가 NULL일 수 있음
	// - SeamlessTravel 중 타이밍 이슈로 값이 유실될 수 있음
	// - 로비에서 Server_SetLobbyNickname으로 “로그인 확정”을 통일한다.

	AMosesStartGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesStartGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[EnterLobby][SV] REJECT (NoStartGM) PC=%s"), *GetNameSafe(this));
		return;
	}

	GM->ServerTravelToLobby();
}

void AMosesPlayerController::Server_SetSelectedCharacterId_Implementation(int32 SelectedId)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(TEXT("Server_SetSelectedCharacterId"));
	if (!PS)
	{
		return;
	}

	// 정책: 캐릭터 2개(1/2)만 허용
	const int32 SafeId = (SelectedId == 2) ? 2 : 1;

	PS->ServerSetSelectedCharacterId(SafeId);

	UE_LOG(LogMosesSpawn, Log, TEXT("[CharSel][SV] SetSelectedCharacterId=%d PC=%s PS=%s"),
		SafeId, *GetNameSafe(this), *GetNameSafe(PS));
}

void AMosesPlayerController::Moses_TestEnterRulesView()
{
	// 로컬만(멀티 PIE에서도 다른 플레이어에 영향 X)
	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			Subsys->EnterRulesView_UIOnly();
		}
	}
}

void AMosesPlayerController::Moses_TestExitRulesView()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			Subsys->ExitRulesView_UIOnly();
		}
	}
}

void AMosesPlayerController::Moses_TestGesture(int32 Count)
{
	if (!IsLocalController())
	{
		return;
	}

	Count = FMath::Clamp(Count, 1, 50);

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			// Count회 SetAnswer 호출로 "답변 시작"을 연속 발생시켜
			// 덱 셔플/재셔플/Back-to-Back 방지를 빠르게 검증한다.
			for (int32 i = 0; i < Count; ++i)
			{
				Subsys->SetAnswer(i, FText::FromString(TEXT("Gesture Test Trigger")));
			}
		}
	}
}

void AMosesPlayerController::Moses_TestSetAnswer(int32 AnswerIndex, const FString& Text)
{
	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			Subsys->SetAnswer(AnswerIndex, FText::FromString(Text));
		}
	}
}

