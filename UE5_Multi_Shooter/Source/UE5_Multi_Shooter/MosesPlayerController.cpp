// ============================================================================
// UE5_Multi_Shooter/MosesPlayerController.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/MosesPlayerController.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesStartGameMode.h"
#include "UE5_Multi_Shooter/Camera/MosesPlayerCameraManager.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "UE5_Multi_Shooter/Lobby/GameMode/MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/Lobby/GameState/MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/Match/GameMode/MosesMatchGameMode.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchHUD.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h"

#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/Persist/MosesMatchRecordStorageSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

// --------------------------------------------------
// Lobby RPC parameter policy (Clamp on server)
// --------------------------------------------------
static constexpr int32 Lobby_MinRoomMaxPlayers = 2;
static constexpr int32 Lobby_MaxRoomMaxPlayers = 4;

// =========================================================
// Constructor
// =========================================================
AMosesPlayerController::AMosesPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = AMosesPlayerCameraManager::StaticClass();
}

// =========================================================
// AActor
// =========================================================

void AMosesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	// =========================================================
	// 🔥 첫 PIE에서 Release 씹힘 방지용 강제 캡처 세팅
	// =========================================================
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* GVC = World->GetGameViewport())
		{
			GVC->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
			GVC->SetMouseLockMode(EMouseLockMode::LockOnCapture);
		}
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	// =========================================================

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

	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();

		BindScopeWeaponEvents_Local();
		return;
	}

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

	// ================= Lobby =================
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

void AMosesPlayerController::ForceGameViewportFocusAndCapture_LocalOnly()
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

	SetIgnoreLookInput(false);
	SetIgnoreMoveInput(false);

	// ✅ 캡처/락은 ViewportClient에서
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* GVC = World->GetGameViewport())
		{
			GVC->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
			GVC->SetMouseLockMode(EMouseLockMode::LockOnCapture);
		}
	}

	// ✅ 포커스는 이 한 줄만 (버전 호환 가장 좋음)
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void AMosesPlayerController::ForceGameViewportFocusAndCapture_NextTick_LocalOnly()
{
	if (!IsLocalController())
	{
		return;
	}

	// 기존 핸들이 있으면 중복 방지
	GetWorldTimerManager().ClearTimer(ForceCaptureNextTickHandle);

	// ✅ 다음 틱에 한 번 더 강제 (첫 PIE에서 가장 효과 좋음)
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsLocalController())
			{
				return;
			}

			ForceGameViewportFocusAndCapture_LocalOnly();
		}));
}


bool AMosesPlayerController::ShouldForceCapture_LocalOnly() const
{
	// 로비는 UI 모드가 정상일 수 있으니 제외하고 싶으면 이렇게
	if (IsLobbyContext())
	{
		return false;
	}

	// 매치/스타트/기타 게임플레이 맵은 강제 캡처
	return true;
}

void AMosesPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ForceCaptureNextTickHandle);

	StopScopeBlurTimer_Local();
	UnbindScopeWeaponEvents_Local();

	bScopeActive_Local = false;
	CachedScopeWeaponData_Local.Reset();
	CachedMatchHUD_Local.Reset();

	Super::EndPlay(EndPlayReason);
}

// =========================================================
// AController
// =========================================================
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
		return;
	}

	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	if (InPawn)
	{
		SetViewTarget(InPawn);
		AutoManageActiveCameraTarget(InPawn);
	}
}

// =========================================================
// APlayerController
// =========================================================
void AMosesPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);

	if (!IsLocalController())
	{
		return;
	}

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

	if (IsLobbyContext())
	{
		bAutoManageActiveCameraTarget = false;
		ApplyLobbyPreviewCamera();
		ApplyLobbyInputMode_LocalOnly();
		ReapplyLobbyInputMode_NextTick_LocalOnly();
		return;
	}

	if (IsStartMap_Local())
	{
		RestoreNonLobbyDefaults_LocalOnly();
		ApplyStartInputMode_LocalOnly();
		ReapplyStartInputMode_NextTick_LocalOnly();
		return;
	}

	RestoreNonLobbyDefaults_LocalOnly();
	RestoreNonLobbyInputMode_LocalOnly();

	if (NewPawn)
	{
		SetViewTarget(NewPawn);
		AutoManageActiveCameraTarget(NewPawn);
	}
}

void AMosesPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LP = GetLocalPlayer())
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

	const bool bOk = GM->Server_RequestReturnToLobbyFromPlayer(this);

	UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] ReturnToLobby Request PC=%s Result=%s"),
		*GetNameSafe(this),
		bOk ? TEXT("OK") : TEXT("FAIL"));
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
// Enter Lobby / Character Select
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
// Persist: Records
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
// Pickup/Headshot Toast (Owner Only)
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

	TryFlushPendingPickupToast_Local();

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

	UE_LOG(LogMosesPickup, Log,
		TEXT("[HEADSHOT_TOAST][CL] ClientRPC Arrived. Text=%s Dur=%.2f World=%s"),
		*Text.ToString(),
		PendingHeadshotToastDuration,
		*GetNameSafe(GetWorld()));

	TryFlushPendingHeadshotToast_Local();

	// ✅ HUD가 아직 없으면: 기존 Pickup RetryTimer를 같이 태워서 HUD 생길 때까지 플러시 보장
	if (bPendingHeadshotToast)
	{
		StartPickupToastRetryTimer_Local();
	}
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
// Context (Local-only)
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
	if (IsMatchMap_Local() || IsStartMap_Local())
	{
		return false;
	}

	if (IsLobbyMap_Local())
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetGameState<AMosesLobbyGameState>() != nullptr);
}

// =========================================================
// Lobby Camera / UI (Local-only)
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
// Input Mode (Local-only)
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
// Toast flush / HUD discovery (Local-only)
// =========================================================
void AMosesPlayerController::TryFlushPendingHeadshotToast_Local()
{
	if (!bPendingHeadshotToast)
	{
		return;
	}

	UMosesMatchHUD* HUD = FindMatchHUD_Local();
	if (!HUD)
	{
		return;
	}

	HUD->ShowHeadshotToast_Local(PendingHeadshotToastText, PendingHeadshotToastDuration);
	bPendingHeadshotToast = false;
}

void AMosesPlayerController::TryFlushPendingPickupToast_Local()
{
	if (!bPendingPickupToast)
	{
		TryFlushPendingHeadshotToast_Local();
		return;
	}

	UMosesMatchHUD* HUD = FindMatchHUD_Local();
	if (!HUD)
	{
		UE_LOG(LogMosesPickup, VeryVerbose,
			TEXT("[PICKUP_TOAST][CL] Flush WAIT. HUD=NULL RetryCount=%d"),
			PickupToastRetryCount);
		return;
	}

	UE_LOG(LogMosesPickup, Log,
		TEXT("[PICKUP_TOAST][CL] Flush OK. Text=%s Dur=%.2f"),
		*PendingPickupToastText.ToString(),
		PendingPickupToastDuration);

	HUD->ShowPickupToast_Local(PendingPickupToastText, PendingPickupToastDuration);

	bPendingPickupToast = false;

	StopPickupToastRetryTimer_Local();
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
// Pickup Toast retry timer (Local-only)
// =========================================================
void AMosesPlayerController::StartPickupToastRetryTimer_Local()
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

	if (World->GetTimerManager().IsTimerActive(TimerHandle_PickupToastRetry))
	{
		return;
	}

	PickupToastRetryCount = 0;

	UE_LOG(LogMosesPickup, Log,
		TEXT("[PICKUP_TOAST][CL] RetryTimer START Interval=%.2f Max=%d"),
		PickupToastRetryIntervalSec,
		MaxPickupToastRetryCount);

	World->GetTimerManager().SetTimer(
		TimerHandle_PickupToastRetry,
		this,
		&ThisClass::HandlePickupToastRetryTimer_Local,
		PickupToastRetryIntervalSec,
		true);
}

void AMosesPlayerController::StopPickupToastRetryTimer_Local()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(TimerHandle_PickupToastRetry))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TimerHandle_PickupToastRetry);

	UE_LOG(LogMosesPickup, Log,
		TEXT("[PICKUP_TOAST][CL] RetryTimer STOP (Pending=%d RetryCount=%d)"),
		bPendingPickupToast ? 1 : 0,
		PickupToastRetryCount);
}

void AMosesPlayerController::HandlePickupToastRetryTimer_Local()
{
	if (!bPendingPickupToast)
	{
		StopPickupToastRetryTimer_Local();
		return;
	}

	++PickupToastRetryCount;

	TryFlushPendingPickupToast_Local();

	if (!bPendingPickupToast)
	{
		return;
	}

	if (PickupToastRetryCount >= MaxPickupToastRetryCount)
	{
		UE_LOG(LogMosesPickup, Warning,
			TEXT("[PICKUP_TOAST][CL] RetryTimer GIVEUP. HUD still NULL. Text=%s World=%s"),
			*PendingPickupToastText.ToString(),
			*GetNameSafe(GetWorld()));

		bPendingPickupToast = false;
		StopPickupToastRetryTimer_Local();

		TryFlushPendingHeadshotToast_Local();
	}
}

// =========================================================
// Scope: Local cosmetic
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

	if (UMosesMatchHUD* HUD = FindMatchHUD_Local())
	{
		HUD->SetScopeVisible_Local(bScopeActive_Local);
	}

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
// Scope: weapon-change safety (Local-only)
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

	if (!bIsSniper && bScopeActive_Local)
	{
		SetScopeActive_Local(false, nullptr);

		if (UMosesMatchHUD* HUD = FindMatchHUD_Local())
		{
			HUD->SetScopeVisible_Local(false);
		}
		return;
	}

	if (UMosesMatchHUD* HUD = FindMatchHUD_Local())
	{
		HUD->SetScopeVisible_Local(bScopeActive_Local);
	}
}
