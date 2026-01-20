#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyWidget.h"
#include "UE5_Multi_Shooter/Lobby/LobbyPreviewActor.h"

#include "Camera/CameraActor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"

void UMosesLobbyLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BindLobbyGameStateEvents();

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Initialize LP=%s World=%s"),
		*GetNameSafe(GetLocalPlayer()),
		*GetNameSafe(GetWorld()));
}

void UMosesLobbyLocalPlayerSubsystem::Deinitialize()
{
	// ✅ 로비 UI 제거(있으면)
	DeactivateLobbyUI();

	// ✅ 로비 전용 타이머 정리
	StopLobbyOnlyTimers();

	// ✅ 바인딩 해제 + 캐시 정리
	UnbindLobbyGameStateEvents();
	LobbyWidget = nullptr;
	ClearLobbyPreviewCache();

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Deinitialize LP=%s"), *GetNameSafe(GetLocalPlayer()));

	Super::Deinitialize();
}

// =========================================================
// Widget registration
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RegisterLobbyWidget(UMosesLobbyWidget* InLobbyWidget)
{
	LobbyWidget = InLobbyWidget;
}

void UMosesLobbyLocalPlayerSubsystem::SetLobbyWidget(UMosesLobbyWidget* InWidget)
{
	LobbyWidget = InWidget;
	RefreshLobbyUI_FromCurrentState();
}

// =========================================================
// UI control
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	// ✅ 매치/다른 맵이면 로비 UI 띄우지 않음 + 로비 타이머 정리
	if (!IsLobbyContext())
	{
		StopLobbyOnlyTimers();
		ClearLobbyPreviewCache();
		return;
	}

	// 멀티 PIE에서 Subsystem이 어느 LP/PC에 붙는지 증명 로그
	{
		ULocalPlayer* LP = GetLocalPlayer();
		UWorld* World = GetWorld();
		APlayerController* PC_FromLP = (LP && World) ? LP->GetPlayerController(World) : nullptr;

		UE_LOG(LogMosesSpawn, Warning, TEXT("[UIFlow][CHECK] ThisSubsys=%s LP=%s PC_FromLP=%s LP.PlayerController(raw)=%s"),
			*GetNameSafe(this),
			*GetNameSafe(LP),
			*GetNameSafe(PC_FromLP),
			*GetNameSafe(LP ? LP->PlayerController : nullptr));
	}

	// 중복 생성 방지 (단, 이미 있으면 Refresh만)
	if (IsLobbyWidgetValid())
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI SKIP (AlreadyValid) Widget=%s"),
			*GetNameSafe(LobbyWidget));

		RefreshLobbyUI_FromCurrentState();
		RefreshLobbyPreviewCharacter_LocalOnly();
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	UGameInstance* GI = LP ? LP->GetGameInstance() : nullptr;
	if (!GI)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No GameInstance)"));
		return;
	}

	UMosesUIRegistrySubsystem* Registry = GI->GetSubsystem<UMosesUIRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No UIRegistrySubsystem)"));
		return;
	}

	FSoftClassPath WidgetPath = Registry->GetLobbyWidgetClassPath();
	if (WidgetPath.IsNull())
	{
		WidgetPath = FSoftClassPath(TEXT("/GF_Lobby/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C"));
		UE_LOG(LogMosesSpawn, Warning, TEXT("[UIFlow] LobbyWidgetPath NULL -> FORCE SET = %s"),
			*WidgetPath.ToString());
	}

	UClass* LoadedClass = WidgetPath.TryLoadClass<UUserWidget>();
	if (!LoadedClass)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (TryLoadClass) Path=%s"),
			*WidgetPath.ToString());
		return;
	}

	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No MosesPC)"));
		return;
	}

	if (!PC->IsLocalController())
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (PC is not local) PC=%s"), *GetNameSafe(PC));
		return;
	}

	UMosesLobbyWidget* NewWidget = CreateWidget<UMosesLobbyWidget>(PC, LoadedClass);
	if (!NewWidget)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (CreateWidget) Class=%s"),
			*GetNameSafe(LoadedClass));
		return;
	}

	LobbyWidget = NewWidget;
	LobbyWidget->AddToViewport(0);

	SetLobbyWidget(LobbyWidget);

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI OK Widget=%s InViewport=%d OwningPC=%s"),
		*GetNameSafe(LobbyWidget),
		LobbyWidget->IsInViewport() ? 1 : 0,
		*GetNameSafe(PC));

	RefreshLobbyPreviewCharacter_LocalOnly();
}

void UMosesLobbyLocalPlayerSubsystem::DeactivateLobbyUI()
{
	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] DeactivateLobbyUI ENTER"));

	if (!LobbyWidget)
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] DeactivateLobbyUI SKIP (Null)"));
		return;
	}

	const bool bWasInViewport = LobbyWidget->IsInViewport();
	LobbyWidget->RemoveFromParent();
	LobbyWidget = nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] DeactivateLobbyUI OK WasInViewport=%d"), bWasInViewport ? 1 : 0);
}

// =========================================================
// Notify
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::NotifyPlayerStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyPlayerStateChanged"));

	if (!IsLobbyContext())
	{
		StopLobbyOnlyTimers();
		ClearLobbyPreviewCache();
		return;
	}

	// [ADD] PS가 늦게 붙는 구간(SeamlessTravel 직후) 대비: 다음 틱에 UI 갱신 시도
	//      (단, 여기서 "닉 전송" 같은 네트워크 행위는 절대 하지 않는다)
	if (AMosesPlayerState* MyPS = GetMosesPS_LocalOnly(); !MyPS)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshLobbyUI_FromCurrentState);
		}
	}

	// =========================================================
	// [MOD] 로그인 의사가 없으면 "닉 자동 생성/자동 전송"을 절대 하지 않는다.
	// - ServerTravel로 로비에 따라온 B를 보호하기 위한 핵심 게이트.
	// - 대신 UI만 갱신하고 끝낸다. (서버가 Dev_Moses를 넣어줄 수 있게 닉을 비워둠)
	// =========================================================
	if (!bLoginSubmitted_Local)
	{
		LobbyPlayerStateChangedEvent.Broadcast();
		RefreshLobbyPreviewCharacter_LocalOnly();
		RefreshLobbyUI_FromCurrentState();
		return;
	}

	// =========================================================
	// 로그인 의사가 있을 때만 AutoNick fallback 허용
	// =========================================================
	{
		AMosesPlayerController* PC = GetMosesPC_LocalOnly();
		AMosesPlayerState* MyPS = GetMosesPS_LocalOnly();

		if (PC && PC->IsLocalController() && MyPS)
		{
			const FString CurrentNick = MyPS->GetPlayerNickName().TrimStartAndEnd();
			if (CurrentNick.IsEmpty() && PendingLobbyNickname_Local.TrimStartAndEnd().IsEmpty())
			{
				const FString NickName = MyPS->GetPlayerName();
				PendingLobbyNickname_Local = NickName;
				bPendingLobbyNicknameSend_Local = true;

				UE_LOG(LogMosesSpawn, Warning, TEXT("[Nick][Auto] Set PendingLobbyNickname_Local='%s' PS=%s"),
					*PendingLobbyNickname_Local, *GetNameSafe(MyPS));
			}
		}
	}

	// =========================================================
	// [MOD] 닉 전송은 "로그인 의사"가 있을 때만 수행
	// =========================================================
	TrySendPendingLobbyNickname_Local();

	// UI 파이프
	LobbyPlayerStateChangedEvent.Broadcast();
	RefreshLobbyPreviewCharacter_LocalOnly();
	RefreshLobbyUI_FromCurrentState();
}


void UMosesLobbyLocalPlayerSubsystem::NotifyRoomStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyRoomStateChanged"));

	LobbyRoomStateChangedEvent.Broadcast();
	RefreshLobbyUI_FromCurrentState();
}

void UMosesLobbyLocalPlayerSubsystem::NotifyJoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyJoinRoomResult Result=%d Room=%s"),
		(int32)Result, *RoomId.ToString());

	LobbyJoinRoomResultEvent.Broadcast(Result, RoomId);
}

// =========================================================
// Login/Nickname (Local-only intent)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestSetLobbyNickname_LocalOnly(const FString& Nick)
{
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	bLoginSubmitted_Local = true;

	PendingLobbyNickname_Local = Nick.TrimStartAndEnd();
	if (PendingLobbyNickname_Local.IsEmpty())
	{
		bPendingLobbyNicknameSend_Local = false;
		return;
	}

	bPendingLobbyNicknameSend_Local = true;
	TrySendPendingLobbyNickname_Local();

	UE_LOG(LogMosesSpawn, Log, TEXT("[Nick][LPS] Pending Nick set='%s' PC=%s"),
		*PendingLobbyNickname_Local, *GetNameSafe(PC));
}

// =========================================================
// Lobby Preview (Public API - Widget에서 호출)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestNextCharacterPreview_LocalOnly()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharNext] ENTER Local=%d"), LocalPreviewSelectedId);

	LocalPreviewSelectedId = (LocalPreviewSelectedId >= 2) ? 1 : 2;

	// 서버(PlayerState)에도 선택값 저장(되돌림 방지)
	if (AMosesPlayerController* PC = GetMosesPC_LocalOnly())
	{
		bPendingSelectedIdServerAck = true;
		PendingSelectedIdServerAck = LocalPreviewSelectedId;

		PC->Server_SetSelectedCharacterId(LocalPreviewSelectedId);
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharNext] Local -> %d"), LocalPreviewSelectedId);

	ApplyPreviewBySelectedId_LocalOnly(LocalPreviewSelectedId);
}

// =========================================================
// Lobby UI refresh (single pipeline)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RefreshLobbyUI_FromCurrentState()
{
	if (!LobbyWidget)
	{
		return;
	}

	AMosesLobbyGameState* GS = GetLobbyGameState();
	AMosesPlayerState* MyPS = GetMosesPS_LocalOnly();
	if (!GS || !MyPS)
	{
		return;
	}

	LobbyWidget->RefreshFromState(GS, MyPS);
}

// =========================================================
// Retry control (Preview refresh - 중복 예약 방지)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestPreviewRefreshRetry_NextTick()
{
	// ✅ 로비가 아니면 재시도 타이머를 걸지 않는다
	if (!IsLobbyContext())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TM = World->GetTimerManager();

	if (TM.TimerExists(PreviewRefreshRetryHandle) &&
		(TM.IsTimerActive(PreviewRefreshRetryHandle) || TM.IsTimerPending(PreviewRefreshRetryHandle)))
	{
		return;
	}

	PreviewRefreshRetryHandle = TM.SetTimerForNextTick(this, &ThisClass::RefreshLobbyPreviewCharacter_LocalOnly);
}

void UMosesLobbyLocalPlayerSubsystem::ClearPreviewRefreshRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreviewRefreshRetryHandle);
	}
}

// =========================================================
// Retry control (Bind GameState - Travel/LateJoin 대비)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestBindLobbyGameStateEventsRetry_NextTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TM = World->GetTimerManager();

	if (TM.TimerExists(BindLobbyGSRetryHandle) &&
		(TM.IsTimerActive(BindLobbyGSRetryHandle) || TM.IsTimerPending(BindLobbyGSRetryHandle)))
	{
		return;
	}

	BindLobbyGSRetryHandle = TM.SetTimerForNextTick(this, &ThisClass::BindLobbyGameStateEvents);
}

void UMosesLobbyLocalPlayerSubsystem::ClearBindLobbyGameStateEventsRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindLobbyGSRetryHandle);
	}
}

// =========================================================
// Internal helpers
// =========================================================

bool UMosesLobbyLocalPlayerSubsystem::IsLobbyContext() const
{
	// ✅ 로비 여부 판정은 "LobbyGameState 존재"로 고정 (네 정책 유지)
	UWorld* World = GetWorld();
	return World && (World->GetGameState<AMosesLobbyGameState>() != nullptr);
}

bool UMosesLobbyLocalPlayerSubsystem::IsLobbyWidgetValid() const
{
	return LobbyWidget && LobbyWidget->IsInViewport();
}

void UMosesLobbyLocalPlayerSubsystem::StopLobbyOnlyTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TM = World->GetTimerManager();
	TM.ClearTimer(PreviewRefreshRetryHandle);
	TM.ClearTimer(BindLobbyGSRetryHandle);
}

void UMosesLobbyLocalPlayerSubsystem::ClearLobbyPreviewCache()
{
	CachedLobbyPreviewActor.Reset();
	bLoggedPreviewWaitOnce = false;
}

// =========================================================
// Lobby Preview (Local-only)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RefreshLobbyPreviewCharacter_LocalOnly()
{
	// ✅ 매치/다른 맵이면 프리뷰 로직을 절대 돌리지 않는다
	if (!IsLobbyContext())
	{
		StopLobbyOnlyTimers();
		ClearLobbyPreviewCache();
		return;
	}

	UWorld* World = GetWorld();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] ENTER World=%s NetMode=%d Subsys=%s LP=%s"),
		*GetPathNameSafe(World),
		World ? (int32)World->GetNetMode() : -1,
		*GetPathNameSafe(this),
		*GetPathNameSafe(GetLocalPlayer()));

	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] SKIP (NoWorld or DedicatedServer)"));
		return;
	}

	AMosesPlayerController* DebugPC = GetMosesPC_LocalOnly();
	AMosesPlayerState* DebugPS = DebugPC ? DebugPC->GetPlayerState<AMosesPlayerState>() : nullptr;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview][DEBUG] PC=%s IsLocal=%d PS=%s"),
		*GetPathNameSafe(DebugPC),
		DebugPC ? (DebugPC->IsLocalController() ? 1 : 0) : 0,
		*GetPathNameSafe(DebugPS));

	if (!DebugPC || !DebugPC->IsLocalController() || !DebugPS)
	{
		if (!bLoggedPreviewWaitOnce)
		{
			bLoggedPreviewWaitOnce = true;
			UE_LOG(LogMosesSpawn, Verbose, TEXT("[CharPreview] WAIT (PC/PS not ready) -> retry next tick"));
		}
		RequestPreviewRefreshRetry_NextTick();
		return;
	}

	ClearPreviewRefreshRetry();

	const AMosesPlayerState* PS = DebugPS;

	int32 SelectedId = PS->GetSelectedCharacterId();

	if (bPendingSelectedIdServerAck && PendingSelectedIdServerAck != INDEX_NONE)
	{
		if (SelectedId != PendingSelectedIdServerAck)
		{
			SelectedId = PendingSelectedIdServerAck;
		}
		else
		{
			bPendingSelectedIdServerAck = false;
			PendingSelectedIdServerAck = INDEX_NONE;
		}
	}

	if (SelectedId != 1 && SelectedId != 2)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] SelectedId invalid (%d) -> clamp to 1"), SelectedId);
		SelectedId = 1;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] Refresh -> Apply SelectedId=%d"), SelectedId);

	const int32 PSId = PS->GetSelectedCharacterId();
	if (PSId == 1 || PSId == 2)
	{
		LocalPreviewSelectedId = PSId;
	}

	ApplyPreviewBySelectedId_LocalOnly(SelectedId);
}

void UMosesLobbyLocalPlayerSubsystem::ApplyPreviewBySelectedId_LocalOnly(int32 SelectedId)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] ApplyPreviewBySelectedId ENTER SelectedId=%d"), SelectedId);

	ALobbyPreviewActor* PreviewActor = GetOrFindLobbyPreviewActor_LocalOnly();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] PreviewActor=%s Cached=%s"),
		*GetNameSafe(PreviewActor),
		*GetNameSafe(CachedLobbyPreviewActor.Get()));

	if (!PreviewActor)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] FAIL: PreviewActor NULL (레벨에 배치 안됨/다른 월드?)"));
		return;
	}

	const ECharacterType Type = (SelectedId == 2) ? ECharacterType::TypeB : ECharacterType::TypeA;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] ApplyVisual Type=%d"), (int32)Type);

	PreviewActor->ApplyVisual(Type);
}

ALobbyPreviewActor* UMosesLobbyLocalPlayerSubsystem::GetOrFindLobbyPreviewActor_LocalOnly()
{
	if (CachedLobbyPreviewActor.IsValid())
	{
		UE_LOG(LogMosesSpawn, Verbose, TEXT("[CharPreview] GetOrFind CACHE HIT=%s"),
			*GetNameSafe(CachedLobbyPreviewActor.Get()));
		return CachedLobbyPreviewActor.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] GetOrFind FAIL: World NULL"));
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ALobbyPreviewActor::StaticClass(), Found);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] GetOrFind Search Count=%d"), Found.Num());

	if (Found.Num() <= 0)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] GetOrFind FAIL: No ALobbyPreviewActor in this level"));
		return nullptr;
	}

	CachedLobbyPreviewActor = Cast<ALobbyPreviewActor>(Found[0]);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] GetOrFind CACHE SET=%s"),
		*GetNameSafe(CachedLobbyPreviewActor.Get()));

	return CachedLobbyPreviewActor.Get();
}

// =========================================================
// Player access helpers
// =========================================================

AMosesPlayerController* UMosesLobbyLocalPlayerSubsystem::GetMosesPC_LocalOnly() const
{
	UWorld* World = GetWorld();
	ULocalPlayer* LP = GetLocalPlayer();
	if (!World || !LP)
	{
		return nullptr;
	}

	return Cast<AMosesPlayerController>(LP->GetPlayerController(World));
}

AMosesPlayerState* UMosesLobbyLocalPlayerSubsystem::GetMosesPS_LocalOnly() const
{
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		return nullptr;
	}

	return PC->GetPlayerState<AMosesPlayerState>();
}

// =========================================================
// Camera helpers (Local-only)
// =========================================================

ACameraActor* UMosesLobbyLocalPlayerSubsystem::FindCameraByTag(const FName& CameraTag) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ACameraActor::StaticClass(), Found);

	for (AActor* A : Found)
	{
		if (A && A->ActorHasTag(CameraTag))
		{
			return Cast<ACameraActor>(A);
		}
	}

	return nullptr;
}

void UMosesLobbyLocalPlayerSubsystem::SetViewTargetToCameraTag(const FName& CameraTag, float BlendTime)
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	APlayerController* PC = LP->GetPlayerController(GetWorld());
	if (!PC)
	{
		return;
	}

	ACameraActor* TargetCam = FindCameraByTag(CameraTag);
	if (!TargetCam)
	{
		return;
	}

	PC->SetViewTargetWithBlend(TargetCam, BlendTime);
}

void UMosesLobbyLocalPlayerSubsystem::SwitchToCamera(ACameraActor* TargetCam, const TCHAR* LogFromTo)
{
	APlayerController* PC = GetLocalPC();
	if (!PC || !TargetCam)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[Camera] Switch failed. PC=%s Cam=%s"),
			*GetNameSafe(PC), *GetNameSafe(TargetCam));
		return;
	}

	PC->SetViewTargetWithBlend(TargetCam, CameraBlendTime);
	UE_LOG(LogMosesSpawn, Log, TEXT("%s"), LogFromTo);
}

APlayerController* UMosesLobbyLocalPlayerSubsystem::GetLocalPC() const
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	return LP->GetPlayerController(GetWorld());
}

// =========================================================
// Lobby GameState bind
// =========================================================

AMosesLobbyGameState* UMosesLobbyLocalPlayerSubsystem::GetLobbyGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<AMosesLobbyGameState>() : nullptr;
}

void UMosesLobbyLocalPlayerSubsystem::BindLobbyGameStateEvents()
{
	UWorld* World = GetWorld();
	if (!World || bBoundToGameState)
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameState();
	if (!LGS)
	{
		if (!bLoggedBindWaitOnce)
		{
			bLoggedBindWaitOnce = true;
			UE_LOG(LogMosesSpawn, Verbose, TEXT("[LPS] BindLobbyGameStateEvents WAIT (GS not ready) -> retry next tick"));
		}

		RequestBindLobbyGameStateEventsRetry_NextTick();
		return;
	}

	ClearBindLobbyGameStateEventsRetry();
	bLoggedBindWaitOnce = false;

	UnbindLobbyGameStateEvents();

	bBoundToGameState = true;
	CachedLobbyGS = LGS;

	// ✅ 실제 GameState 델리게이트가 있다면 여기서 Bind (프로젝트 정의에 맞게 연결)
	// 예)
	// LGS->OnRoomStateChanged.AddUObject(this, &ThisClass::NotifyRoomStateChanged);
	// LGS->OnSomething.AddUObject(this, ...);

	RefreshLobbyUI_FromCurrentState();
}

void UMosesLobbyLocalPlayerSubsystem::UnbindLobbyGameStateEvents()
{
	if (!bBoundToGameState)
	{
		return;
	}

	AMosesLobbyGameState* LGS = CachedLobbyGS.Get();
	if (!LGS)
	{
		bBoundToGameState = false;
		CachedLobbyGS.Reset();
		return;
	}

	// ✅ 실제 GameState 델리게이트가 있다면 여기서 Unbind
	// 예)
	// LGS->OnRoomStateChanged.RemoveAll(this);

	bBoundToGameState = false;
	CachedLobbyGS.Reset();

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][LPS] UnbindLobbyGameStateEvents OK"));
}

// =========================================================
// Pending nickname send (Local-only)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::TrySendPendingLobbyNickname_Local()
{
	if (!bPendingLobbyNicknameSend_Local)
	{
		return;
	}

	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TrySendPendingLobbyNickname_Local);
		}
		return;
	}

	if (PendingLobbyNickname_Local.IsEmpty())
	{
		bPendingLobbyNicknameSend_Local = false;
		return;
	}

	// Standalone/ListenServer(권한 있음) => RPC 대신 직접 세팅
	if (PC->HasAuthority())
	{
		PS->ServerSetPlayerNickName(PendingLobbyNickname_Local);
		PS->ServerSetLoggedIn(true);

		bPendingLobbyNicknameSend_Local = false;

		UE_LOG(LogMosesSpawn, Log, TEXT("[Nick][LPS] Applied locally (Authority) Nick='%s' PS=%s"),
			*PendingLobbyNickname_Local, *GetNameSafe(PS));

		RefreshLobbyUI_FromCurrentState();
		return;
	}

	// 진짜 클라일 때만 RPC
	if (!PC->GetNetConnection())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TrySendPendingLobbyNickname_Local);
		}
		return;
	}

	PC->Server_SetLobbyNickname(PendingLobbyNickname_Local);
	bPendingLobbyNicknameSend_Local = false;

	UE_LOG(LogMosesSpawn, Log, TEXT("[Nick][LPS] Sent Server_SetLobbyNickname '%s' PC=%s PS=%s"),
		*PendingLobbyNickname_Local, *GetNameSafe(PC), *GetNameSafe(PS));
}

void UMosesLobbyLocalPlayerSubsystem::NotifyLobbyNicknameChanged(const FString& NewNickname)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[Nick][LPS] NotifyLobbyNicknameChanged NewNick='%s' World=%s"),
		*NewNickname,
		*GetNameSafe(GetWorld()));

	// ✅ 매치/다른 맵이면 로비 UI 갱신 파이프 자체를 돌리지 않음
	if (!IsLobbyContext())
	{
		return;
	}

	// ✅ Widget이 이미 LPS 이벤트에 바인딩되어 있으니, "PS 바뀜" 이벤트를 재사용해서 한 파이프만 유지
	LobbyPlayerStateChangedEvent.Broadcast();

	// ✅ 즉시 갱신도 한번 더(위젯이 아직 바인딩 전인 타이밍 대비)
	RefreshLobbyUI_FromCurrentState();
}
