#include "UE5_Multi_Shooter/MosesPlayerController.h"

#include "UE5_Multi_Shooter/Match/GameMode/MosesMatchGameMode.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesStartGameMode.h"

#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "UE5_Multi_Shooter/Lobby/GameMode/MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/Lobby/GameState/MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchHUD.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfCheatManager.h"
#include "UE5_Multi_Shooter/Perf/MosesPerfTestSubsystem.h"

#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/Persist/MosesMatchRecordStorageSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"

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
	CheatClass = UMosesPerfCheatManager::StaticClass();
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

	// Match => Cursor OFF
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

	// Lobby => Cursor ON
	if (IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly();
		ReapplyLobbyInputMode_NextTick_LocalOnly();
		return;
	}

	// Start => Cursor ON
	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();
		return;
	}

	// Other => Cursor OFF
	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	if (NewPawn)
	{
		SetViewTarget(NewPawn);
		AutoManageActiveCameraTarget(NewPawn);
	}
}

// =========================================================
// Debug Exec
// =========================================================
void AMosesPlayerController::gas_DumpTags() {}
void AMosesPlayerController::gas_DumpAttr() {}

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
// Local: Pending Nick
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

	if (!GetNetConnection() || !PlayerState)
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
// Lifecycle
// =========================================================
void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	// Match => 커서 OFF
	if (IsMatchMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (APawn* LocalPawn = GetPawn())
		{
			SetViewTarget(LocalPawn);
			AutoManageActiveCameraTarget(LocalPawn);
		}

		BindScopeWeaponEvents_Local();
		return;
	}

	// Start => 커서 ON
	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();

		BindScopeWeaponEvents_Local();
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

		BindScopeWeaponEvents_Local();
		return;
	}

	// Lobby => 커서 ON
	bAutoManageActiveCameraTarget = false;
	ActivateLobbyUI_LocalOnly();
	ApplyLobbyInputMode_LocalOnly();
	ApplyLobbyPreviewCamera();
	ReapplyLobbyInputMode_NextTick_LocalOnly();

	GetWorldTimerManager().SetTimer(
		LobbyPreviewCameraTimerHandle,
		this,
		&ThisClass::ApplyLobbyPreviewCamera,
		LobbyPreviewCameraDelay,
		false);

	BindScopeWeaponEvents_Local();
}

void AMosesPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopScopeBlurTimer_Local();
	UnbindScopeWeaponEvents_Local();

	bScopeActive_Local = false;
	CachedScopeWeaponData_Local.Reset();
	CachedMatchHUD_Local.Reset();

	Super::EndPlay(EndPlayReason);
}

void AMosesPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsLocalController())
	{
		return;
	}

	if (IsMatchMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		if (InPawn)
		{
			SetViewTarget(InPawn);
			AutoManageActiveCameraTarget(InPawn);
		}
		return;
	}

	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();
		return;
	}

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
}

void AMosesPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (!IsLocalController())
	{
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (LP)
	{
		if (UMosesLobbyLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			Subsys->NotifyPlayerStateChanged();
		}
	}

	BindScopeWeaponEvents_Local();
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

// =========================================================
// Replication
// =========================================================
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
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyRPC][SV] JoinRoom REJECT (InvalidRoomId) PC=%s"), *GetNameSafe(this));
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
	}

	EMosesRoomJoinResult Result = EMosesRoomJoinResult::Ok;
	LGS->Server_JoinRoomWithResult(PS, RoomId, Result);

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
		return;
	}

	if (!PS->GetRoomId().IsValid())
	{
		return;
	}

	PS->ServerSetReady(bInReady);

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
}

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
// Server-side Travel helpers
// =========================================================
void AMosesPlayerController::DoServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		return;
	}

	if (AMosesLobbyGameMode* LobbyGM = Cast<AMosesLobbyGameMode>(GM))
	{
		LobbyGM->TravelToMatch();
	}
}

void AMosesPlayerController::DoServerTravelToLobby()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM)
	{
		return;
	}

	if (AMosesMatchGameMode* MatchGM = Cast<AMosesMatchGameMode>(GM))
	{
		MatchGM->TravelToLobby();
	}
}

// =========================================================
// Server: StartGame → Lobby 진입 요청 처리
// =========================================================
void AMosesPlayerController::Server_RequestEnterLobby_Implementation(const FString& Nickname)
{
	(void)Nickname;

	if (!HasAuthority())
	{
		return;
	}

	AMosesStartGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMosesStartGameMode>() : nullptr;
	if (!GM)
	{
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
}

// =========================================================
// Context 판정 (Local-only)
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
	// Match/Start에서는 절대 Lobby로 판정되면 안 됨 (Travel 타이밍 흔들림 방지)
	if (IsMatchMap_Local() || IsStartMap_Local())
	{
		return false;
	}

	// 1순위: 맵 이름
	if (IsLobbyMap_Local())
	{
		return true;
	}

	// 2순위: GameState 타입 보조
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetGameState<AMosesLobbyGameState>() != nullptr);
}

// =========================================================
// Lobby Camera / UI
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
}

void AMosesPlayerController::ActivateLobbyUI_LocalOnly()
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	if (UMosesLobbyLocalPlayerSubsystem* LobbySubsys = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
	{
		LobbySubsys->ActivateLobbyUI();
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
// InputMode
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

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsLocalController())
			{
				return;
			}

			if (IsMatchMap_Local())
			{
				RestoreNonLobbyInputMode_LocalOnly();
				return;
			}

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

			if (IsStartMap_Local())
			{
				ApplyStartInputMode_LocalOnly();
			}
		}));
}

// =========================================================
// Shared getters
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
// Match Camera Policy (Local-only)
// =========================================================
void AMosesPlayerController::ApplyMatchCameraPolicy_LocalOnly(const TCHAR* Reason)
{
	(void)Reason;

	if (!IsLocalController())
	{
		return;
	}

	// MatchMap이면 무조건 NonLobby 정책 강제
	if (IsMatchMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		RestoreNonLobbyInputMode_LocalOnly();

		SetIgnoreLookInput(false);
		SetIgnoreMoveInput(false);

		if (APawn* LocalPawn = GetPawn())
		{
			SetViewTarget(LocalPawn);
			AutoManageActiveCameraTarget(LocalPawn);
		}
		return;
	}

	// Lobby/Start에서는 건드리지 않는다
	if (IsLobbyContext() || IsStartMap_Local())
	{
		return;
	}

	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	SetIgnoreLookInput(false);
	SetIgnoreMoveInput(false);

	if (!GetPawn())
	{
		RetryApplyMatchCameraPolicy_NextTick_LocalOnly();
	}
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

			if (IsLobbyContext() || IsStartMap_Local())
			{
				return;
			}

			ApplyMatchCameraPolicy_LocalOnly(TEXT("NextTickRetry"));
		}));
}

// =========================================================
// [PERSIST] Records
// =========================================================
void AMosesPlayerController::RequestMatchRecords_Local(const FString& NicknameFilter, int32 MaxCount)
{
	if (!IsLocalController())
	{
		return;
	}

	MaxCount = FMath::Clamp(MaxCount, 1, 200);
	Server_RequestMatchRecords(NicknameFilter, MaxCount);
}

void AMosesPlayerController::Server_RequestMatchRecords_Implementation(const FString& NicknameFilter, int32 MaxCount)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesMatchRecordStorageSubsystem* Storage = GI->GetSubsystem<UMosesMatchRecordStorageSubsystem>();
	if (!Storage)
	{
		return;
	}

	const FString MyPid = PS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens);
	const FString MyNick = PS->GetPlayerNickName();

	TArray<FMosesMatchRecordSummary> Records;
	Storage->LoadRecordSummaries_Server(MyPid, MyNick, NicknameFilter, MaxCount, Records);

	Client_ReceiveMatchRecords(Records);
}

void AMosesPlayerController::Client_ReceiveMatchRecords_Implementation(const TArray<FMosesMatchRecordSummary>& Records)
{
	OnMatchRecordsReceivedBP.Broadcast(Records);
}

// =========================================================
// Pickup Toast (Owner Only)
// =========================================================
void AMosesPlayerController::Client_ShowPickupToast_OwnerOnly_Implementation(const FText& Text, float DurationSec)
{
	if (!IsLocalController())
	{
		return;
	}

	bPendingPickupToast = true;
	PendingPickupToastText = Text;
	PendingPickupToastDuration = FMath::Clamp(DurationSec, 0.2f, 10.0f);

	UE_LOG(LogMosesPickup, Log,
		TEXT("[PICKUP_TOAST][CL] ClientRPC Arrived. Text=%s Dur=%.2f World=%s"),
		*Text.ToString(),
		PendingPickupToastDuration,
		*GetNameSafe(GetWorld()));

	// 즉시 flush 시도 (HUD 있으면 바로 뜸)
	TryFlushPendingPickupToast_Local();

	// HUD가 없어서 못 띄웠으면 재시도 타이머 시작
	if (bPendingPickupToast)
	{
		StartPickupToastRetryTimer_Local();
	}
}

void AMosesPlayerController::Client_ShowHeadshotToast_OwnerOnly_Implementation(const FText& Text, float DurationSec)
{
	if (!IsLocalController())
	{
		return;
	}

	bPendingHeadshotToast = true;
	PendingHeadshotToastText = Text;
	PendingHeadshotToastDuration = FMath::Clamp(DurationSec, 0.2f, 10.0f);

	TryFlushPendingHeadshotToast_Local();
}

void AMosesPlayerController::TryFlushPendingHeadshotToast_Local()
{
	if (!bPendingHeadshotToast)
	{
		return;
	}

	UMosesMatchHUD* HUD = FindMatchHUD_Local();
	if (!HUD)
	{
		// HUD 생성/교체 타이밍이면 pending 유지
		return;
	}

	HUD->ShowHeadshotToast_Local(PendingHeadshotToastText, PendingHeadshotToastDuration);

	bPendingHeadshotToast = false;
}

void AMosesPlayerController::TryFlushPendingPickupToast_Local()
{
	if (!bPendingPickupToast)
	{
		// [MOD] Headshot만 pending일 수도 있으니 flush 시도
		TryFlushPendingHeadshotToast_Local();
		return;
	}

	UMosesMatchHUD* HUD = FindMatchHUD_Local();
	if (!HUD)
	{
		UE_LOG(LogMosesPickup, VeryVerbose,
			TEXT("[PICKUP_TOAST][CL] Flush WAIT. HUD=NULL RetryCount=%d"),
			PickupToastRetryCount);

		// Pending은 유지. 타이머가 계속 재시도해서 HUD 준비되면 뜸.
		return;
	}

	UE_LOG(LogMosesPickup, Log,
		TEXT("[PICKUP_TOAST][CL] Flush OK. Text=%s Dur=%.2f"),
		*PendingPickupToastText.ToString(),
		PendingPickupToastDuration);

	HUD->ShowPickupToast_Local(PendingPickupToastText, PendingPickupToastDuration);

	bPendingPickupToast = false;

	// 토스트 성공했으니 재시도 타이머는 종료
	StopPickupToastRetryTimer_Local();

	// 픽업 flush 후에도 headshot pending이 남아있을 수 있음
	TryFlushPendingHeadshotToast_Local();
}

UMosesMatchHUD* AMosesPlayerController::FindMatchHUD_Local()
{
	if (CachedMatchHUD_Local.IsValid())
	{
		return CachedMatchHUD_Local.Get();
	}

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UMosesMatchHUD::StaticClass(), false);

	for (UUserWidget* W : Widgets)
	{
		UMosesMatchHUD* HUD = Cast<UMosesMatchHUD>(W);
		if (!HUD)
		{
			continue;
		}

		if (HUD->GetOwningPlayer() == this)
		{
			CachedMatchHUD_Local = HUD;
			return HUD;
		}
	}

	return nullptr;
}

// =========================================================
// [SCOPE] Local cosmetic
// =========================================================
UMosesCombatComponent* AMosesPlayerController::FindCombatComponent_Local() const
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	return PS ? PS->FindComponentByClass<UMosesCombatComponent>() : nullptr;
}

UMosesCameraComponent* AMosesPlayerController::FindMosesCameraComponent_Local() const
{
	APawn* P = GetPawn();
	return P ? P->FindComponentByClass<UMosesCameraComponent>() : nullptr;
}

bool AMosesPlayerController::CanUseScope_Local(const UMosesWeaponData*& OutWeaponData) const
{
	OutWeaponData = nullptr;

	if (!IsLocalController())
	{
		return false;
	}

	UMosesCombatComponent* Combat = FindCombatComponent_Local();
	if (!Combat)
	{
		return false;
	}

	const FGameplayTag WeaponId = Combat->GetEquippedWeaponId();
	if (!WeaponId.IsValid())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return false;
	}

	UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return false;
	}

	const UMosesWeaponData* WeaponData = Registry->ResolveWeaponData(WeaponId);
	if (!WeaponData)
	{
		return false;
	}

	// 스나이퍼만 허용(Tag 기반)
	if (WeaponData->WeaponId != FMosesGameplayTags::Get().Weapon_Sniper_A)
	{
		return false;
	}

	OutWeaponData = WeaponData;
	return true;
}

void AMosesPlayerController::Scope_OnPressed_Local()
{
	const UMosesWeaponData* WeaponData = nullptr;
	if (!CanUseScope_Local(WeaponData))
	{
		return;
	}

	SetScopeActive_Local(true, WeaponData);
}

void AMosesPlayerController::Scope_OnReleased_Local()
{
	if (!bScopeActive_Local)
	{
		return;
	}

	SetScopeActive_Local(false, nullptr);
}

void AMosesPlayerController::SetScopeActive_Local(bool bActive, const UMosesWeaponData* WeaponData)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bScopeActive_Local == bActive)
	{
		return;
	}

	bScopeActive_Local = bActive;

	// HUD 토글
	if (UMosesMatchHUD* HUD = FindMatchHUD_Local())
	{
		HUD->SetScopeVisible_Local(bScopeActive_Local);
	}

	// 카메라 FOV/블러
	if (UMosesCameraComponent* Cam = FindMosesCameraComponent_Local())
	{
		if (bScopeActive_Local)
		{
			const float ScopedFOV = WeaponData ? WeaponData->ScopeFOV : 45.0f;
			Cam->SetSniperScopeActive_Local(true, ScopedFOV);
			Cam->SetScopeBlurStrength_Local(0.0f);
		}
		else
		{
			Cam->SetSniperScopeActive_Local(false, 45.0f);
			Cam->SetScopeBlurStrength_Local(0.0f);
		}
	}

	if (bScopeActive_Local)
	{
		CachedScopeWeaponData_Local = WeaponData;
		StartScopeBlurTimer_Local();
	}
	else
	{
		StopScopeBlurTimer_Local();
		CachedScopeWeaponData_Local.Reset();
	}
}

void AMosesPlayerController::StartScopeBlurTimer_Local()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(ScopeBlurTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		ScopeBlurTimerHandle,
		this,
		&ThisClass::TickScopeBlur_Local,
		ScopeBlurUpdateInterval,
		true);
}

void AMosesPlayerController::StopScopeBlurTimer_Local()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(ScopeBlurTimerHandle))
	{
		World->GetTimerManager().ClearTimer(ScopeBlurTimerHandle);
	}
}

void AMosesPlayerController::TickScopeBlur_Local()
{
	if (!bScopeActive_Local)
	{
		return;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	UMosesCameraComponent* Cam = FindMosesCameraComponent_Local();
	if (!Cam)
	{
		return;
	}

	const UMosesWeaponData* WeaponData = CachedScopeWeaponData_Local.Get();

	const float BlurMin = WeaponData ? WeaponData->ScopeBlur_Min : 0.0f;
	const float BlurMax = WeaponData ? WeaponData->ScopeBlur_Max : 0.85f;
	const float SpeedRef = WeaponData ? FMath::Max(1.0f, WeaponData->ScopeBlur_SpeedRef) : 600.0f;

	const float Speed2D = P->GetVelocity().Size2D();
	const float Factor = FMath::Clamp(Speed2D / SpeedRef, 0.0f, 1.0f);
	const float Blur = FMath::Lerp(BlurMin, BlurMax, Factor);

	Cam->SetScopeBlurStrength_Local(Blur);
}

// =========================================================
// [SCOPE] weapon-change safety (local only)
// =========================================================
void AMosesPlayerController::BindScopeWeaponEvents_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	UMosesCombatComponent* Combat = FindCombatComponent_Local();
	if (!Combat)
	{
		return;
	}

	if (CachedCombatForScope_Local.Get() == Combat)
	{
		return;
	}

	UnbindScopeWeaponEvents_Local();

	CachedCombatForScope_Local = Combat;
	Combat->OnEquippedChanged.AddUObject(this, &ThisClass::HandleEquippedChanged_ScopeLocal);

	// Safety: 스나이퍼가 아니면 스코프는 OFF
	const FGameplayTag CurWeaponId = Combat->GetEquippedWeaponId();
	const bool bIsSniperNow = (CurWeaponId == FMosesGameplayTags::Get().Weapon_Sniper_A);

	if (!bIsSniperNow && bScopeActive_Local)
	{
		SetScopeActive_Local(false, nullptr);
	}
}

void AMosesPlayerController::UnbindScopeWeaponEvents_Local()
{
	if (UMosesCombatComponent* Combat = CachedCombatForScope_Local.Get())
	{
		Combat->OnEquippedChanged.RemoveAll(this);
	}

	CachedCombatForScope_Local.Reset();
}

void AMosesPlayerController::HandleEquippedChanged_ScopeLocal(int32 SlotIndex, FGameplayTag WeaponId)
{
	(void)SlotIndex;

	if (!IsLocalController())
	{
		return;
	}

	const bool bIsSniper = (WeaponId == FMosesGameplayTags::Get().Weapon_Sniper_A);

	// 스나이퍼가 아니면 스코프는 무조건 OFF
	if (!bIsSniper && bScopeActive_Local)
	{
		SetScopeActive_Local(false, nullptr);

		if (UMosesMatchHUD* HUD = FindMatchHUD_Local())
		{
			HUD->SetScopeVisible_Local(false);
		}
		return;
	}

	// 스나이퍼면 HUD는 현재 ScopeActive 상태를 반영
	if (UMosesMatchHUD* HUD = FindMatchHUD_Local())
	{
		HUD->SetScopeVisible_Local(bScopeActive_Local);
	}
}

void AMosesPlayerController::StartPickupToastRetryTimer_Local()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(TimerHandle_PickupToastRetry))
		{
			return; // 이미 돌고 있음
		}

		PickupToastRetryCount = 0;

		UE_LOG(LogMosesPickup, Log,
			TEXT("[PICKUP_TOAST][CL] RetryTimer START Interval=%.2f Max=%d"),
			PickupToastRetryIntervalSec,
			MaxPickupToastRetryCount);

		World->GetTimerManager().SetTimer(
			TimerHandle_PickupToastRetry,
			this,
			&AMosesPlayerController::HandlePickupToastRetryTimer_Local,
			PickupToastRetryIntervalSec,
			true);
	}
}

void AMosesPlayerController::StopPickupToastRetryTimer_Local()
{
	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(TimerHandle_PickupToastRetry))
		{
			World->GetTimerManager().ClearTimer(TimerHandle_PickupToastRetry);

			UE_LOG(LogMosesPickup, Log,
				TEXT("[PICKUP_TOAST][CL] RetryTimer STOP (Pending=%d RetryCount=%d)"),
				bPendingPickupToast ? 1 : 0,
				PickupToastRetryCount);
		}
	}
}

void AMosesPlayerController::HandlePickupToastRetryTimer_Local()
{
	// Pending이 해제됐으면 종료
	if (!bPendingPickupToast)
	{
		StopPickupToastRetryTimer_Local();
		return;
	}

	++PickupToastRetryCount;

	// HUD 준비됐는지 확인하고 가능하면 flush
	TryFlushPendingPickupToast_Local();

	// flush 성공하면 TryFlush에서 Stop 처리함
	if (!bPendingPickupToast)
	{
		return;
	}

	// 안전장치: 일정 시간 내 HUD가 안 뜨면 포기(로그로 증거 남김)
	if (PickupToastRetryCount >= MaxPickupToastRetryCount)
	{
		UE_LOG(LogMosesPickup, Warning,
			TEXT("[PICKUP_TOAST][CL] RetryTimer GIVEUP. HUD still NULL. Text=%s World=%s"),
			*PendingPickupToastText.ToString(),
			*GetNameSafe(GetWorld()));

		// Pending은 버리거나 유지 중 선택인데, 실무에선 버려서 무한 대기 방지
		bPendingPickupToast = false;

		StopPickupToastRetryTimer_Local();

		// headshot pending만 남았을 수 있으니 한 번 더
		TryFlushPendingHeadshotToast_Local();
	}
}

void AMosesPlayerController::Server_RequestReturnToLobby_Implementation()
{
	if (!HasAuthority())
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[RESULT][CL] Server_RequestReturnToLobby called on non-authority PC=%s"),
			*GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AMosesMatchGameMode* GM = World->GetAuthGameMode<AMosesMatchGameMode>();
	if (!GM)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] ReturnToLobby FAIL (NoMatchGM) PC=%s"),
			*GetNameSafe(this));
		return;
	}

	// Server only 승인 + Travel
	const bool bOk = GM->Server_RequestReturnToLobbyFromPlayer(this);

	UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] ReturnToLobby Request PC=%s Result=%s"),
		*GetNameSafe(this),
		bOk ? TEXT("OK") : TEXT("FAIL"));
}

void AMosesPlayerController::Perf_Dump()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>())
		{
			Subsys->DumpPerfBindingState(this);
		}
	}
}

void AMosesPlayerController::Perf_Marker(int32 MarkerIndex)
{
	// Marker 이동은 로컬 연출/측정용이므로 로컬에서만 OK
	if (!IsLocalController())
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>())
		{
			// 네 기존 Bootstrap 로직과 동일하게 “Index→MarkerId” 매핑이 필요함.
			// 가장 깔끔한 방법: Subsystem에 "TryMoveByIndex"를 추가하거나,
			// Bootstrap이 가진 MarkerId를 Subsystem에 이미 등록하니,
			// 여기서는 MarkerId를 문자열로 입력받는 방식(perf_marker Marker01)로 바꾸는 것도 추천.
		}
	}
}

void AMosesPlayerController::Perf_Spawn(int32 Count)
{
	// 스폰은 서버 권위
	if (HasAuthority())
	{
		Server_PerfSpawn(Count); // 서버에서 호출해도 Implementation으로 들어감
	}
	else
	{
		Server_PerfSpawn(Count);
	}
}

void AMosesPlayerController::Perf_MeasureBegin(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec)
{
	Server_PerfMeasureBegin(MeasureId, MarkerId, SpawnCount, TrialIndex, TrialTotal, DurationSec);
}

void AMosesPlayerController::Server_PerfMeasureBegin_Implementation(const FString& MeasureId, const FString& MarkerId, int32 SpawnCount, int32 TrialIndex, int32 TrialTotal, float DurationSec)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>())
		{
			Subsys->BeginMeasure(this, FName(*MeasureId), FName(*MarkerId), SpawnCount, TrialIndex, TrialTotal, DurationSec);
		}
	}
}

void AMosesPlayerController::Perf_MeasureEnd()
{
	Server_PerfMeasureEnd();
}

void AMosesPlayerController::Server_PerfMeasureEnd_Implementation()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>())
		{
			Subsys->EndMeasure(this);
		}
	}
}

void AMosesPlayerController::Server_PerfMarker_Implementation(const FName MarkerId)
{
	APawn* LocalPawn = GetPawn(); // [MOD] Pawn -> LocalPawn (C4458 fix)
	if (!LocalPawn)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	FTransform MarkerTransform;
	if (!Subsys->TryGetMarkerTransform(MarkerId, MarkerTransform)) // [MOD] FindMarker_ForServer 제거
	{
		UE_LOG(LogMosesAuth, Warning, TEXT("[PERF][MOVE][SV] FAIL Marker not found Id=%s"), *MarkerId.ToString());
		return;
	}

	const FVector Loc = MarkerTransform.GetLocation();
	const FRotator Rot = MarkerTransform.Rotator();

	// ✅ 서버 권위 텔레포트
	LocalPawn->TeleportTo(Loc, Rot, false, true);

	// ✅ 시점까지 확실히 맞추고 싶으면 (원하면 유지)
	SetControlRotation(Rot);

	LocalPawn->ForceNetUpdate();

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][MOVE][SV] OK Marker=%s Pawn=%s Loc=%s Rot=%s"),
		*MarkerId.ToString(),
		*GetNameSafe(LocalPawn),
		*Loc.ToString(),
		*Rot.ToString());
}

void AMosesPlayerController::Server_PerfSpawn_Implementation(int32 Count)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UMosesPerfTestSubsystem* Subsys = GI->GetSubsystem<UMosesPerfTestSubsystem>();
	if (!Subsys)
	{
		return;
	}

	const bool bOK = Subsys->TrySpawnZombies_ServerAuthority(Count);

	UE_LOG(LogMosesAuth, Warning,
		TEXT("[PERF][SPAWN][SV] %s Count=%d"),
		bOK ? TEXT("OK") : TEXT("FAIL"),
		Count);
}
