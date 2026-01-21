// =========================================================
// MosesPlayerController.cpp (FULL)
// - [MOD] Lobby 판정 안정화(IsLobbyContext): 맵 이름 기반 1순위
// - [ADD] IsLobbyMap_Local / IsMatchMap_Local
// - [MOD] BeginPlay/OnPossess/ClientRestart에서 커서 정책 강제
// - [ADD] Lobby BeginPlay에서 NextTick 재적용(ReapplyLobbyInputMode_NextTick_LocalOnly)
// =========================================================

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

	// [ADD] 매치 맵이면 커서/입력은 무조건 GameOnly(커서 OFF)로 강제
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

	// [MOD] 로비 판정은 맵 이름 기반 1순위(IsLobbyContext 내부)
	if (IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly(); // [ADD] Restart 타이밍에서 입력 덮어쓰기 방지(커서 ON 확정)
		return;
	}

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

	// [ADD] 매치 맵이면 커서 OFF를 무조건 고정
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

	// [MOD] NonLobby 판정(로비가 아니면)에서도 기존대로 복구
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

	// Lobby Context
	bAutoManageActiveCameraTarget = false;

	ActivateLobbyUI_LocalOnly();
	ApplyLobbyInputMode_LocalOnly();
	ApplyLobbyPreviewCamera();

	// [ADD] Travel 직후/Slate 포커스/엔진 덮어쓰기 타이밍에서 커서가 한번 꺼지는 케이스 방지
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

	// [ADD] 매치 맵이면 커서 OFF 강제
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

	if (IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly(); // [MOD] Possess 타이밍에서도 커서 ON 확정
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

	UE_LOG(LogTemp, Warning, TEXT("[TEST][Cam] PC=%s Pawn=%s ViewTarget=%s AutoManage=%d Lobby=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InPawn),
		*GetNameSafe(GetViewTarget()),
		bAutoManageActiveCameraTarget ? 1 : 0,
		IsLobbyContext() ? 1 : 0);
}

void AMosesPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// [FIX] PS가 실제로 유효해진 시점에 Subsystem에게 "누가" 바뀌었는지 함께 알려준다.
	//       PS=None 타이밍에서 UI가 먼저 읽는 문제를 차단하는 1차 게이트.
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

	Subsys->NotifyPlayerStateChanged(); // 기존 유지
	// Subsys->NotifyPlayerStateChanged(PS); // (권장) Subsystem에 오버로드 있으면 이쪽이 더 안전
}

void AMosesPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
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
// Lobby/Match Context 판정 (Local-only)
// =========================================================

bool AMosesPlayerController::IsLobbyMap_Local() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// [ADD] Travel 직후에도 안정적인 판정: 현재 레벨 이름
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

bool AMosesPlayerController::IsLobbyContext() const
{
	// [MOD] 1순위: 맵 이름 기반(Travel/복제 타이밍에 흔들리지 않음)
	if (IsLobbyMap_Local())
	{
		return true;
	}

	// 2순위(보조): GameState 타입 기반(예: 테스트/PIE 상황에서 맵 이름 다를 때)
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

	// [ADD] 다음 프레임에 "한 번 더" 적용해, Travel 직후 덮어쓰기를 이긴다.
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

// =========================================================
// [ADD] MosesPlayerController.cpp - 함수 전문만
// =========================================================

void AMosesPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();

	if (!IsLocalController())
	{
		return;
	}

	/**
	 * 개발자 주석:
	 * - BeginPlay/ClientRestart/OnPossess는 SeamlessTravel / RestartPlayer 타이밍에서
	 *   "Pawn이 아직 없는 상태"로 호출될 수 있다.
	 * - BeginPlayingState는 로컬 플레이어가 실제로 플레이 상태로 들어가는 비교적 안정 타이밍이므로
	 *   여기서 한 번 더 "매치 카메라 정책"을 강제한다.
	 */
	ApplyMatchCameraPolicy_LocalOnly(TEXT("BeginPlayingState"));
}

void AMosesPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	if (!IsLocalController())
	{
		return;
	}

	/**
	 * 개발자 주석(매우 중요):
	 * - OnPossess는 서버에서 주로 의미가 있고,
	 *   클라이언트에서는 Pawn이 "복제되어 도착"하는 시점이 더 신뢰할 수 있다.
	 * - 특히 SeamlessTravel/Restart 흐름에서
	 *   ClientRestart(NewPawn=nullptr) -> (조금 뒤) OnRep_Pawn() 순으로 오는 케이스가 흔하다.
	 * - 따라서 여기서 "Pawn 카메라 복구"를 확정하면
	 *   월드 고정 시점(로비 ViewTarget 잔존 등)이 구조적으로 사라진다.
	 */
	ApplyMatchCameraPolicy_LocalOnly(TEXT("OnRep_Pawn"));
}

void AMosesPlayerController::ApplyMatchCameraPolicy_LocalOnly(const TCHAR* Reason)
{
	if (!IsLocalController())
	{
		return;
	}

	// 매치/비로비 컨텍스트에서만 동작 (로비면 건드리지 않는다)
	// - 현재 프로젝트 정책상 StartGame 레벨은 커서 OFF여도 큰 문제 없으므로,
	//   "로비가 아니면" NonLobby 정책을 우선 적용한다.
	const bool bIsLobby = IsLobbyContext();
	if (bIsLobby)
	{
		return;
	}

	// NonLobby 기본 정책 강제
	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	APawn* LocalPawn = GetPawn();
	if (IsValid(LocalPawn))
	{
		// ✅ 핵심: Pawn이 생긴 순간 무조건 ViewTarget/Pawn으로 복구
		SetViewTarget(LocalPawn);
		AutoManageActiveCameraTarget(LocalPawn);

		UE_LOG(LogTemp, Warning,
			TEXT("[CAM][PC][%s] ApplyMatchCameraPolicy OK Pawn=%s ViewTarget=%s AutoManage=%d Map=%s"),
			Reason ? Reason : TEXT("None"),
			*GetNameSafe(LocalPawn),
			*GetNameSafe(GetViewTarget()),
			bAutoManageActiveCameraTarget ? 1 : 0,
			*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));

		// 혹시 남아있는 재시도 타이머가 있으면 정리
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(MatchCameraRetryTimerHandle);
		}
		return;
	}

	// Pawn이 아직 없다면 "다음 틱에 한 번 더" 재시도
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

	// 다음 프레임에 1회 재시도 (무한 루프 방지: 타이머 핸들을 재사용)
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsLocalController())
			{
				return;
			}

			// 로비로 돌아간 경우에는 중단
			if (IsLobbyContext())
			{
				return;
			}

			ApplyMatchCameraPolicy_LocalOnly(TEXT("NextTickRetry"));
		}));
}
