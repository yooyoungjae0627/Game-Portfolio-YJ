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
// ctor
// =========================================================

AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();

	/**
	 * 주의:
	 * - bAutoManageActiveCameraTarget을 ctor에서 영구로 끄면 매치에서 Pawn 카메라 자동 복구가 깨질 수 있음.
	 * - 로비에서만 끄고, 로비가 아니면 반드시 원복한다. (BeginPlay / RestoreNonLobbyDefaults_LocalOnly)
	 */
}

// =========================================================
// Camera / Restart
// =========================================================

void AMosesPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);

	if (!IsLocalController())
	{
		return;
	}

	// =========================================================
	// InputMode / Cursor 정책
	// - Match : Cursor OFF (GameOnly)
	// - Lobby : Cursor ON  (GameAndUI)
	// - Start : Cursor ON  (GameAndUI)  ✅ 요구사항
	// =========================================================

	// Match => 무조건 OFF
	if (IsMatchMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (NewPawn)
		{
			SetViewTarget(NewPawn);
			AutoManageActiveCameraTarget(NewPawn);
		}
		return;
	}

	// Lobby => ON
	if (IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly();
		ReapplyLobbyInputMode_NextTick_LocalOnly();
		return;
	}

	// ✅ Start => ON
	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();
		return;
	}

	// Other NonLobby => OFF
	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	if (NewPawn)
	{
		SetViewTarget(NewPawn);
		AutoManageActiveCameraTarget(NewPawn);
	}

	UE_LOG(LogTemp, Warning, TEXT("[CAM][PC] ClientRestart -> ForcePawnViewTarget Pawn=%s ViewTarget=%s AutoManage=%d"),
		*GetNameSafe(NewPawn),
		*GetNameSafe(GetViewTarget()),
		bAutoManageActiveCameraTarget ? 1 : 0);
}

// =========================================================
// Debug Exec
// =========================================================

void AMosesPlayerController::gas_DumpTags()
{
}

void AMosesPlayerController::gas_DumpAttr()
{
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
	TrySendPendingLobbyNickname_Local();
}

// 이미 존재하는 Server_SendLobbyChat의 구현을 "서버 권위 GameState 위임"으로 통일
void AMosesPlayerController::Server_SendLobbyChat_Implementation(const FString& Text)
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<AMosesLobbyGameState>() : nullptr;
	if (!LGS)
	{
		return;
	}

	LGS->Server_AddChatMessage(PS, Text);
}

void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	// Match => 커서 OFF 고정
	if (IsMatchMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (APawn* LocalPawn = GetPawn())
		{
			SetViewTarget(LocalPawn);
			AutoManageActiveCameraTarget(LocalPawn);
		}
		return;
	}

	// ✅ Start => 커서 ON
	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();

		UE_LOG(LogMosesSpawn, Warning, TEXT("[Start][PC] BeginPlay -> Cursor ON Map=%s"),
			*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));

		return;
	}

	// NonLobby => 커서 OFF
	if (!IsLobbyContext())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (APawn* LocalPawn = GetPawn())
		{
			SetViewTarget(LocalPawn);
			AutoManageActiveCameraTarget(LocalPawn);
		}

		UE_LOG(LogTemp, Warning, TEXT("[CAM][PC] BeginPlay NonLobby -> Restore + ForcePawnViewTarget Pawn=%s ViewTarget=%s AutoManage=%d"),
			*GetNameSafe(GetPawn()),
			*GetNameSafe(GetViewTarget()),
			bAutoManageActiveCameraTarget ? 1 : 0);

		return;
	}

	// Lobby Context => 커서 ON
	bAutoManageActiveCameraTarget = false;

	ActivateLobbyUI_LocalOnly();
	ApplyLobbyInputMode_LocalOnly();
	ApplyLobbyPreviewCamera();

	ReapplyLobbyInputMode_NextTick_LocalOnly();

	GetWorldTimerManager().SetTimer(
		LobbyPreviewCameraTimerHandle,
		this,
		&AMosesPlayerController::ApplyLobbyPreviewCamera,
		LobbyPreviewCameraDelay,
		false
	);
}

void AMosesPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AMosesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsLocalController())
	{
		return;
	}

	// Match => 커서 OFF
	if (IsMatchMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (InPawn)
		{
			SetViewTarget(InPawn);
			AutoManageActiveCameraTarget(InPawn);
		}

		UE_LOG(LogTemp, Warning, TEXT("[TEST][Cam] (Match) PC=%s Pawn=%s ViewTarget=%s AutoManage=%d"),
			*GetNameSafe(this),
			*GetNameSafe(InPawn),
			*GetNameSafe(GetViewTarget()),
			bAutoManageActiveCameraTarget ? 1 : 0);

		return;
	}

	// ✅ Start => 커서 ON
	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();
		return;
	}

	// Lobby => 커서 ON
	if (IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly();
		ReapplyLobbyInputMode_NextTick_LocalOnly();
	}
	else
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (InPawn)
		{
			SetViewTarget(InPawn);
			AutoManageActiveCameraTarget(InPawn);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[TEST][Cam] PC=%s Pawn=%s ViewTarget=%s AutoManage=%d Lobby=%d Start=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InPawn),
		*GetNameSafe(GetViewTarget()),
		bAutoManageActiveCameraTarget ? 1 : 0,
		IsLobbyContext() ? 1 : 0,
		IsStartMap_Local() ? 1 : 0);
}

void AMosesPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (!IsLocalController())
	{
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>();
	if (!Subsys)
	{
		return;
	}

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[PC][CL] OnRep_PlayerState PS=%s"), *GetNameSafe(PS));

	Subsys->NotifyPlayerStateChanged();
}

void AMosesPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesPlayerController, SelectedCharacterId);
	DOREPLIFETIME(AMosesPlayerController, bRep_IsReady);
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

	PS->EnsurePersistentId_Server();

	if (!PS->IsLoggedIn())
	{
		const FString PlayerNickName = PS->GetPlayerNickName();

		PS->ServerSetPlayerNickName(PlayerNickName);
		PS->ServerSetLoggedIn(true);

		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[LobbyRPC][SV] AutoLoginBeforeJoin Nick=%s Pid=%s PC=%s"),
			*PlayerNickName,
			*PS->GetPersistentId().ToString(),
			*GetNameSafe(this));
	}

	EMosesRoomJoinResult Result = EMosesRoomJoinResult::Ok;
	const bool bOk = LGS->Server_JoinRoomWithResult(PS, RoomId, Result);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyRPC][SV] JoinRoom %s Room=%s Result=%d PC=%s Pid=%s LoggedIn=%d"),
		bOk ? TEXT("OK") : TEXT("FAIL"),
		*RoomId.ToString(),
		(int32)Result,
		*GetNameSafe(this),
		*PS->GetPersistentId().ToString(),
		PS->IsLoggedIn() ? 1 : 0);

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

	if (PS->IsRoomHost())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] SetReady REJECT (Host cannot Ready) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	if (!PS->GetRoomId().IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] SetReady REJECT (NotInRoom) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	PS->ServerSetReady(bInReady);
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterServer_SetReady"));

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

	PS->EnsurePersistentId_Server();

	const FString Clean = Nick.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return;
	}

	PS->ServerSetPlayerNickName(Clean);
	PS->ServerSetLoggedIn(true);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[NickSV] PC=%s PS=%s Nick=%s Pid=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PS),
		*Clean,
		*PS->GetPersistentId().ToString());
}

// =========================================================
// JoinRoom Result (Server → Client)
// =========================================================

void AMosesPlayerController::Client_JoinRoomResult_Implementation(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
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

// =========================================================
// Pending Nickname send helper (Local-only)
// =========================================================

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

	if (!GetNetConnection())
	{
		return;
	}

	if (!PlayerState)
	{
		return;
	}

	if (PendingLobbyNickname_Local.IsEmpty())
	{
		bPendingLobbyNicknameSend_Local = false;
		return;
	}

	Server_SetLobbyNickname(PendingLobbyNickname_Local);
	bPendingLobbyNicknameSend_Local = false;
}

// =========================================================
// Server-side Travel helpers
// =========================================================

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

	AGameModeBase* GM = GetWorld()->GetAuthGameMode();
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

// =========================================================
// Lobby/Match/Start Context 판정 (Local-only)
// =========================================================

bool AMosesPlayerController::IsLobbyMap_Local() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	return (CurrentMapName == LobbyMapName);
}

bool AMosesPlayerController::IsMatchMap_Local() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	return (CurrentMapName == MatchMapName);
}

bool AMosesPlayerController::IsStartMap_Local() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	return (CurrentMapName == StartMapName);
}

bool AMosesPlayerController::IsLobbyContext() const
{
	// 1순위: 맵 이름 기반
	if (IsLobbyMap_Local())
	{
		return true;
	}

	// 2순위: GameState 타입 기반 보조
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetGameState<AMosesLobbyGameState>() != nullptr);
}

// =========================================================
// Lobby Context / Camera / UI (Local-only)
// =========================================================

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

	bAutoManageActiveCameraTarget = false;
	SetViewTargetWithBlend(TargetCam, 0.0f);

	UE_LOG(LogTemp, Verbose, TEXT("[LobbyCam] Applied ViewTarget=%s"), *GetNameSafe(TargetCam));
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
	bAutoManageActiveCameraTarget = true;

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(LobbyPreviewCameraTimerHandle);
	}

	if (IsLocalController())
	{
		if (APawn* LocalPawn = GetPawn())
		{
			SetViewTarget(LocalPawn);
			AutoManageActiveCameraTarget(LocalPawn);
		}
	}
}

// =========================================================
// InputMode (Lobby/Start/NonLobby)
// =========================================================

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

void AMosesPlayerController::ApplyStartInputMode_LocalOnly()
{
	if (!IsLocalController())
	{
		return;
	}

	// 개발자 주석:
	// - Start 모드는 UI 입력 중심(닉 입력/버튼 클릭)이므로 커서 ON이 필요하다.
	// - Lobby와 동일 정책(GameAndUI)을 사용한다.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	SetInputMode(Mode);

	UE_LOG(LogMosesSpawn, Verbose, TEXT("[Start][PC] ApplyStartInputMode -> Cursor ON Map=%s"),
		*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
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

void AMosesPlayerController::ReapplyLobbyInputMode_NextTick_LocalOnly()
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsLocalController())
			{
				return;
			}

			// 매치로 이미 넘어갔으면 커서 OFF
			if (IsMatchMap_Local())
			{
				RestoreNonLobbyInputMode_LocalOnly();
				return;
			}

			// 로비면 커서 ON 유지
			if (IsLobbyContext())
			{
				ApplyLobbyInputMode_LocalOnly();
			}
		}));
}

void AMosesPlayerController::ReapplyStartInputMode_NextTick_LocalOnly()
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsLocalController())
			{
				return;
			}

			// Start가 아니면 건드리지 않는다
			if (IsStartMap_Local())
			{
				ApplyStartInputMode_LocalOnly();
			}
		}));
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

// =========================================================
// Server: StartGame → Lobby 진입 요청 처리
// =========================================================

void AMosesPlayerController::Server_RequestEnterLobby_Implementation(const FString& Nickname)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesStartGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesStartGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[EnterLobby][SV] REJECT (NoStartGM) PC=%s"), *GetNameSafe(this));
		return;
	}

	GM->ServerTravelToLobby();
}

// =========================================================
// Server: 캐릭터 선택 처리
// =========================================================

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

	const int32 SafeId = (SelectedId == 2) ? 2 : 1;

	PS->ServerSetSelectedCharacterId(SafeId);

	UE_LOG(LogMosesSpawn, Log, TEXT("[CharSel][SV] SetSelectedCharacterId=%d PC=%s PS=%s"),
		SafeId, *GetNameSafe(this), *GetNameSafe(PS));
}

void AMosesPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	if (!IsLocalController())
	{
		return;
	}

	ApplyMatchCameraPolicy_LocalOnly(TEXT("OnRep_Pawn"));
}

void AMosesPlayerController::ApplyMatchCameraPolicy_LocalOnly(const TCHAR* Reason)
{
	if (!IsLocalController())
	{
		return;
	}

	// ✅ 핵심 보호:
	// - Lobby/Start에서는 커서/입력/카메라 정책을 건드리지 않는다.
	// - Start는 UI 중심 화면이므로 NonLobby(커서 OFF)로 묶이면 UX가 깨진다.
	if (IsLobbyContext() || IsStartMap_Local())
	{
		return;
	}

	// NonLobby 기본 정책 강제
	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	APawn* LocalPawn = GetPawn();
	if (IsValid(LocalPawn))
	{
		SetViewTarget(LocalPawn);
		AutoManageActiveCameraTarget(LocalPawn);

		UE_LOG(LogTemp, Warning,
			TEXT("[CAM][PC][%s] ApplyMatchCameraPolicy OK Pawn=%s ViewTarget=%s AutoManage=%d Map=%s"),
			Reason ? Reason : TEXT("None"),
			*GetNameSafe(LocalPawn),
			*GetNameSafe(GetViewTarget()),
			bAutoManageActiveCameraTarget ? 1 : 0,
			*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));

		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(MatchCameraRetryTimerHandle);
		}
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[CAM][PC][%s] ApplyMatchCameraPolicy RETRY (Pawn=None) ViewTarget=%s AutoManage=%d Map=%s"),
		Reason ? Reason : TEXT("None"),
		*GetNameSafe(GetViewTarget()),
		bAutoManageActiveCameraTarget ? 1 : 0,
		*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));

	RetryApplyMatchCameraPolicy_NextTick_LocalOnly();
}

void AMosesPlayerController::RetryApplyMatchCameraPolicy_NextTick_LocalOnly()
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsLocalController())
			{
				return;
			}

			// Lobby/Start로 돌아간 경우에는 중단
			if (IsLobbyContext() || IsStartMap_Local())
			{
				return;
			}

			ApplyMatchCameraPolicy_LocalOnly(TEXT("NextTickRetry"));
		}));
}
