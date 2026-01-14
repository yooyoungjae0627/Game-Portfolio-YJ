// MosesLobbyLocalPlayerSubsystem.cpp

#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyWidget.h"

#include "UE5_Multi_Shooter/Dialogue/MosesDialogueCommandDetector.h"
#include "UE5_Multi_Shooter/Dialogue/CommandLexiconDA.h"

#include "UE5_Multi_Shooter/Lobby/LobbyPreviewActor.h"

#include "UE5_Multi_Shooter/Dialogue/Components/MosesGestureDeckComponent.h"

#include "Camera/CameraActor.h"

#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#include "Blueprint/UserWidget.h"

#include "TimerManager.h"

namespace MosesLobbyLogGuard
{
	static FORCEINLINE bool ChangedBool(bool& InOutCached, bool NewValue)
	{
		if (InOutCached == NewValue) return false;
		InOutCached = NewValue;
		return true;
	}

	template<typename T>
	static FORCEINLINE bool ChangedValue(T& InOutCached, const T& NewValue)
	{
		if (InOutCached == NewValue) return false;
		InOutCached = NewValue;
		return true;
	}
}

using ThisClass = UMosesLobbyLocalPlayerSubsystem;

// =========================================================
// Engine
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// - 기본값은 Default 로비 화면
	LobbyViewMode = ELobbyViewMode::Default;

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Initialize LP=%s World=%s"),
		*GetNameSafe(GetLocalPlayer()),
		*GetNameSafe(GetWorld()));

	BindLobbyGameStateEvents();
}

void UMosesLobbyLocalPlayerSubsystem::Deinitialize()
{
	DeactivateLobbyUI();
	ExitRulesView_UIOnly();

	ClearPreviewRefreshRetry();
	ClearEnterDialogueRetry();
	ClearBindLobbyGameStateEventsRetry();

	UnbindLobbyGameStateEvents();   // ✅ 델리게이트 중복/유령 바인딩 방지
	CachedLobbyPreviewActor = nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Deinitialize LP=%s"), *GetNameSafe(GetLocalPlayer()));

	Super::Deinitialize();
}

// =========================================================
// Lobby GS binding
// =========================================================

AMosesLobbyGameState* UMosesLobbyLocalPlayerSubsystem::GetLobbyGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<AMosesLobbyGameState>() : nullptr;
}

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
		// ✅ Travel 직후 / LateJoin에서 GS가 아직 없을 수 있음 → NextTick 재시도
		if (!bLoggedBindWaitOnce)
		{
			bLoggedBindWaitOnce = true;
			UE_LOG(LogMosesSpawn, Verbose, TEXT("[LPS] BindLobbyGameStateEvents WAIT (GS not ready) -> retry next tick"));
		}

		RequestBindLobbyGameStateEventsRetry_NextTick();
		return;
	}

	// ✅ GS를 찾았으면 재시도 타이머 제거
	ClearBindLobbyGameStateEventsRetry();
	bLoggedBindWaitOnce = false;

	// ---- 이하 기존 로직 유지 ----
	UnbindLobbyGameStateEvents();

	bBoundToGameState = true;
	CachedLobbyGS = LGS;

	LGS->OnPhaseChanged.AddUObject(this, &UMosesLobbyLocalPlayerSubsystem::HandlePhaseChanged);

	// ✅ UFUNCTION 진입점으로 수렴
	LGS->OnDialogueStateChanged.AddUObject(this, &UMosesLobbyLocalPlayerSubsystem::HandleGameStateDialogueChanged);

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][LPS] BindLobbyGameStateEvents OK (LateJoinRecoverReady)"));

	// ✅ LateJoin / 초기 접속 즉시 1회 적용
	HandlePhaseChanged(LGS->GetGamePhase());
	HandleDialogueStateChanged(LGS->GetDialogueNetState());

	// ✅ 추가: 바인딩 완료 시점에 UI 한 번 강제 갱신(초기 Room/Chat/Name 표시)
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
		// 캐시는 죽었지만 플래그는 살아있을 수 있으니 정리
		bBoundToGameState = false;
		CachedLobbyGS = nullptr;
		return;
	}

	LGS->OnPhaseChanged.RemoveAll(this);
	LGS->OnDialogueStateChanged.RemoveAll(this);

	bBoundToGameState = false;
	CachedLobbyGS = nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][LPS] UnbindLobbyGameStateEvents OK"));
}

// =========================================================
// UI control
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	// ✅ 멀티 PIE에서 "이 Subsystem이 어느 LP에 붙었는지" 즉시 증명 로그
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

	// - 디버깅용: 경로가 실제 로딩되는지 즉시 확인
	{
		const FSoftClassPath TestPath(TEXT("/GF_Lobby/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C"));
		UClass* TestClass = TestPath.TryLoadClass<UUserWidget>();

		UE_LOG(LogMosesSpawn, Warning, TEXT("[PATH PROOF] %s : %s -> Class=%s"),
			TestClass ? TEXT("OK") : TEXT("FAIL"),
			*TestPath.ToString(),
			*GetNameSafe(TestClass));
	}

	// - 중복 생성 방지
	if (IsLobbyWidgetValid())
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI SKIP (AlreadyValid) Widget=%s"),
			*GetNameSafe(LobbyWidget));
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
		// - GF 디버깅/임시 강제 경로
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

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI LoadedClass=%s"), *GetNameSafe(LoadedClass));

	// - LocalPlayerSubsystem에서 만들면 "LP의 PlayerController"를 OwningPlayer로 넣는 게 안전하다.
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No MosesPC)"));
		return;
	}

	// ✅ 멀티 PIE에서 PC0 고정/잘못된 PC를 초기에 차단
	if (!PC->IsLocalController())
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (PC is not local) PC=%s"), *GetNameSafe(PC));
		return;
	}

	LobbyWidget = CreateWidget<UMosesLobbyWidget>(PC, LoadedClass);
	if (!LobbyWidget)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (CreateWidget) Class=%s"),
			*GetNameSafe(LoadedClass));
		return;
	}

	LobbyWidget->AddToViewport(0);

	// ✅ 핵심: Subsystem에 Widget 등록 (재생성/복구/상태전파 기준점)
	SetLobbyWidget(LobbyWidget);

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI OK Widget=%s InViewport=%d OwningPC=%s"),
		*GetNameSafe(LobbyWidget),
		LobbyWidget->IsInViewport() ? 1 : 0,
		*GetNameSafe(PC));

	// - UI가 뜨는 순간 현재 PS 상태(SelectedCharacterId)에 맞춰 프리뷰도 동기화
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
// Lobby UI Refresh (핵심 파이프)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RefreshLobbyUI_FromCurrentState()
{
	// 1) UI 없으면 만들 수 있는 상황이면 생성 시도(로비 들어왔는데 아직 UI 없을 때 대비)
	// - 여기서 무조건 만들면 "매치에서도 UI가 뜨는" 사고 가능.
	// - 너 프로젝트는 Phase 기반 연출이 있으니, 안전하게 "LobbyPhase일 때만" 켜는 방식 권장.
	// (최소 구현: 여기서 ActivateLobbyUI()를 호출하지 않고 그냥 패스)
	if (!IsLobbyWidgetValid())
	{
		// TODO(옵션): Phase 기반으로 안전할 때만 ActivateLobbyUI();
	}

	if (!LobbyWidget)
	{
		return;
	}

	AMosesLobbyGameState* GS = GetLobbyGameState();
	AMosesPlayerState* MyPS = GetMosesPS_LocalOnly();

	// 2) 아직 PS/GS 준비 안 됐으면 다음 틱에 알아서 Notify/Retry로 다시 들어올 것이므로 그냥 리턴
	if (!GS || !MyPS)
	{
		return;
	}

	// 3) ✅ 여기서 “로비 UI 갱신”을 위젯에게 위임
	LobbyWidget->RefreshFromState(GS, MyPS);
}

// =========================================================
// Notify
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::NotifyPlayerStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyPlayerStateChanged"));

	// ✅ 로그인 의사가 없는 클라는 AutoNick을 만들지도, 보내지도 않는다.
	if (!bLoginSubmitted_Local)
	{
		RefreshLobbyUI_FromCurrentState();
		return;
	}

	// ✅ 로그인 의사가 있을 때만 AutoNick fallback 허용
	{
		AMosesPlayerController* PC = GetMosesPC_LocalOnly();
		AMosesPlayerState* MyPS = GetMosesPS_LocalOnly();

		if (PC && PC->IsLocalController() && MyPS)
		{
			const FString CurrentNick = MyPS->GetPlayerNickName().TrimStartAndEnd();
			if (CurrentNick.IsEmpty() && PendingLobbyNickname_Local.TrimStartAndEnd().IsEmpty())
			{
				FString AutoNick = MyPS->GetPlayerName().TrimStartAndEnd();

				if (AutoNick.IsEmpty())
				{
					ULocalPlayer* LP = GetLocalPlayer();
					const int32 LocalIdx = LP ? LP->GetControllerId() : 0;
					AutoNick = FString::Printf(TEXT("Guest_%d"), LocalIdx);
				}

				PendingLobbyNickname_Local = AutoNick;
				bPendingLobbyNicknameSend_Local = true;

				UE_LOG(LogMosesSpawn, Warning, TEXT("[Nick][Auto] Set PendingLobbyNickname_Local='%s' PS=%s"),
					*PendingLobbyNickname_Local, *GetNameSafe(MyPS));
			}
		}
	}

	TrySendPendingLobbyNickname_Local();
	LobbyPlayerStateChangedEvent.Broadcast();
	RefreshLobbyPreviewCharacter_LocalOnly();
	RefreshLobbyUI_FromCurrentState();
}


void UMosesLobbyLocalPlayerSubsystem::NotifyRoomStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyRoomStateChanged"));

	LobbyRoomStateChangedEvent.Broadcast();

	// ✅ RoomList/ChatHistory 등 “GS 기반 데이터” 갱신
	RefreshLobbyUI_FromCurrentState();
}

void UMosesLobbyLocalPlayerSubsystem::NotifyJoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyJoinRoomResult Result=%d Room=%s"),
		(int32)Result, *RoomId.ToString());

	LobbyJoinRoomResultEvent.Broadcast(Result, RoomId);
}

// =========================================================
// Lobby Preview (Public API - Widget에서 호출)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestNextCharacterPreview_LocalOnly()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharNext] ENTER Local=%d"), LocalPreviewSelectedId);

	LocalPreviewSelectedId = (LocalPreviewSelectedId >= 2) ? 1 : 2;

	// ✅ 서버(PlayerState)에도 선택값을 저장(방 생성/입장 등으로 Refresh가 돌 때 되돌아가는 현상 방지)
	if (AMosesPlayerController* PC = GetMosesPC_LocalOnly())
	{
		bPendingSelectedIdServerAck = true;
		PendingSelectedIdServerAck = LocalPreviewSelectedId;

		// ✅ 아래 RPC는 4) PlayerController 코드 추가가 필요
		PC->Server_SetSelectedCharacterId(LocalPreviewSelectedId);
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharNext] Local -> %d"), LocalPreviewSelectedId);

	ApplyPreviewBySelectedId_LocalOnly(LocalPreviewSelectedId);
}

// =========================================================
// Internal helpers
// =========================================================

bool UMosesLobbyLocalPlayerSubsystem::IsLobbyWidgetValid() const
{
	return (LobbyWidget != nullptr) && LobbyWidget->IsInViewport();
}

// =========================================================
// Lobby Preview (Local only)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RefreshLobbyPreviewCharacter_LocalOnly()
{
	UWorld* World = GetWorld();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] ENTER World=%s NetMode=%d Subsys=%s LP=%s"),
		*GetPathNameSafe(World),
		World ? (int32)World->GetNetMode() : -1,
		*GetPathNameSafe(this),
		*GetPathNameSafe(GetLocalPlayer()));

	// - DS는 화면 없음
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] SKIP (NoWorld or DedicatedServer)"));
		return;
	}

	// ✅ 디버그: 지금 이 타이밍에 Subsystem이 보고 있는 PC/PS 상태를 먼저 출력
	AMosesPlayerController* DebugPC = GetMosesPC_LocalOnly();
	AMosesPlayerState* DebugPS = DebugPC ? DebugPC->GetPlayerState<AMosesPlayerState>() : nullptr;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview][DEBUG] PC=%s IsLocal=%d PS=%s"),
		*GetPathNameSafe(DebugPC),
		DebugPC ? (DebugPC->IsLocalController() ? 1 : 0) : 0,
		*GetPathNameSafe(DebugPS));

	// ✅ Travel/Join 직후 PS가 몇 틱 NULL일 수 있음 (정상) -> NextTick 재시도 1개만 예약
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

	// ✅ 성공했으면 재시도 타이머는 즉시 제거 (스팸 제거)
	ClearPreviewRefreshRetry();

	const AMosesPlayerState* PS = DebugPS;

	int32 SelectedId = PS->GetSelectedCharacterId();
	
	// ✅ 로컬에서 먼저 바뀐 선택값이 서버에 아직 반영되기 전이면,
	//    PS 기반 Refresh가 "기본값"으로 되돌리는 문제를 막기 위해 로컬 Pending 값을 우선 적용한다.
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

	// - PS 값이 유효하면 로컬값도 동기화
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
		*GetNameSafe(CachedLobbyPreviewActor));

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
	// ✅ 캐시가 있고 유효하면 그대로 반환
	if (CachedLobbyPreviewActor && IsValid(CachedLobbyPreviewActor))
	{
		UE_LOG(LogMosesSpawn, Verbose, TEXT("[CharPreview] GetOrFind CACHE HIT=%s"),
			*GetNameSafe(CachedLobbyPreviewActor));
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
		*GetNameSafe(CachedLobbyPreviewActor));

	return CachedLobbyPreviewActor.Get();
}

// =========================================================
// Player access helper
// =========================================================

AMosesPlayerController* UMosesLobbyLocalPlayerSubsystem::GetMosesPC_LocalOnly() const
{
	UWorld* World = GetWorld();
	ULocalPlayer* LP = GetLocalPlayer();
	if (!World || !LP)
	{
		return nullptr;
	}

	return Cast<AMosesPlayerController>(LP->GetPlayerController(World)); // ★ 정석
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
// Internal
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::SetLobbyViewMode(ELobbyViewMode NewMode)
{
	if (LobbyViewMode == NewMode)
	{
		return;
	}

	LobbyViewMode = NewMode;

	// - Subsystem은 "상태 변경"을 이벤트로 방송만 한다.
	// - 실제 UI 갱신은 Widget에서 수행.
	OnLobbyViewModeChanged.Broadcast(LobbyViewMode);
}

// =========================================================
// Camera helpers
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
	if (!GetLocalPlayer())
	{
		return nullptr;
	}

	return GetLocalPlayer()->GetPlayerController(GetWorld());
}

void UMosesLobbyLocalPlayerSubsystem::ApplyRulesViewToWidget(bool bEnable)
{
	if (!LobbyWidget)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[RulesView] ApplyRulesViewToWidget: LobbyWidget is null"));
		return;
	}

	LobbyWidget->SetRulesViewMode(bEnable);
}

// =========================================================
// UI Only: Rules View (Camera in Subsystem, UI in Widget via Event)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::EnterRulesView_UIOnly()
{
	// ✅ 사용자 트리거 진입: Greeting 포함
	EnterOrRecoverRulesView_UIOnly(true, /*bPlayGreeting*/true);
}

void UMosesLobbyLocalPlayerSubsystem::RecoverRulesView_UIOnly(bool bEnable)
{
	// ✅ 복구: Greeting 없이 상태만 맞춘다
	EnterOrRecoverRulesView_UIOnly(bEnable, /*bPlayGreeting*/false);
}

void UMosesLobbyLocalPlayerSubsystem::EnterOrRecoverRulesView_UIOnly(bool bEnable, bool bPlayGreeting)
{
	// ---- OFF 복구는 Exit 로직으로 수렴 ----
	if (!bEnable)
	{
		ExitRulesView_UIOnly();
		return;
	}

	// ✅ 중복 진입 방지
	if (bInRulesView)
	{
		// 이미 RulesView면, 복구 목적이면 세션만 한 번 반영해도 됨
		if (!bPlayGreeting)
		{
			ApplySessionToWidget();
		}
		return;
	}

	bInRulesView = true;

	UE_LOG(LogMosesSpawn, Log, TEXT("[RulesView] EnterOrRecover ENTER Greeting=%d"), bPlayGreeting ? 1 : 0);

	// 1) 카메라 전환
	ACameraActor* DialogueCam = FindCameraByTag(DialogueCameraTag);
	if (!DialogueCam)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Camera] Dialogue camera not found. Tag=%s"), *DialogueCameraTag.ToString());
	}
	else
	{
		SwitchToCamera(DialogueCam, TEXT("[Camera] LobbyPreview -> DialogueCamera"));
	}

	// 2) UI 원복은 위젯이 한다: 이벤트만 쏜다
	RulesViewModeChangedEvent.Broadcast(true);

	// 3) UI 전환 직후(다음 틱) 세션/필요 시 Greeting 적용
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([this, bPlayGreeting]()
			{
				ApplySessionToWidget();

				if (bPlayGreeting)
				{
					ApplyGreetingIfNeeded();
				}
			});
	}
}


void UMosesLobbyLocalPlayerSubsystem::ApplyGreetingIfNeeded()
{
	if (bGreetingShownThisEntry)
	{
		return;
	}
	bGreetingShownThisEntry = true;

	if (LobbyWidget)
	{
		LobbyWidget->ShowDialogueBubble_UI(/*bPlayFadeIn*/false);
		LobbyWidget->SetSubtitleVisibility(true);
		LobbyWidget->SetDialogueBubbleText_UI(GreetingText);
	}

	PlayRulesMetaHumanGesture_LocalOnly();

	UE_LOG(LogMosesSpawn, Log, TEXT("[Day2][Greeting] Shown"));
}

void UMosesLobbyLocalPlayerSubsystem::ApplySessionToWidget()
{
	if (!LobbyWidget)
	{
		return;
	}

	// Day2: 하단 내 발화 / 마이크 상태는 “표시만”
	LobbyWidget->SetMySpeechText_UI(CachedMySpeechText);
	LobbyWidget->SetMicState_UI((int32)MicState);
}

void UMosesLobbyLocalPlayerSubsystem::ExitRulesView_UIOnly()
{
	// ✅ 중복 종료 방지
	if (!bInRulesView)
	{
		return;
	}

	bInRulesView = false;

	UE_LOG(LogMosesSpawn, Log, TEXT("[RulesView] Exit"));

	// 1) 카메라 복귀
	ACameraActor* LobbyCam = FindCameraByTag(LobbyPreviewCameraTag);
	if (!LobbyCam)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Camera] Lobby preview camera not found. Tag=%s"), *LobbyPreviewCameraTag.ToString());
	}
	else
	{
		SwitchToCamera(LobbyCam, TEXT("[Camera] DialogueCamera -> LobbyPreview"));
	}

	// ✅ Exit 시 로컬 세션 상태 초기화 (Day2: 다음 진입에서 다시 Greeting)
	bGreetingShownThisEntry = false;
	CachedMySpeechText = FText::GetEmpty();
	MicState = EMosesMicState::Off;

	// Exit 시 제스처 정지(테스트 설명서: Exit 안정성)
	StopRulesMetaHumanGesture_LocalOnly();

	// 다음 진입 때 LineIndex 트리거 재시작
	CachedLastGestureLineIndex = INDEX_NONE;

	// 캐시 리셋은 선택(레벨에 NPC가 바뀔 수 있으면 Reset 권장)
	// CachedRulesMetaHumanActor.Reset();

	// ✅ 중앙 자막 Clear (Exit DoD)
	ClearDialogueUITexts();

	// ✅ UI 모드 OFF 브로드캐스트
	RulesViewModeChangedEvent.Broadcast(false);

	// ✅ 하단 UI 기본값 반영(혹시 RulesView가 다시 켜졌을 때 대비)
	ApplySessionToWidget();

	UE_LOG(LogMosesSpawn, Log, TEXT("[Day2] ExitRulesView -> Clear UI"));
}

void UMosesLobbyLocalPlayerSubsystem::ClearDialogueUITexts()
{
	if (!LobbyWidget)
	{
		return;
	}

	LobbyWidget->SetDialogueBubbleText_UI(FText::GetEmpty());
	LobbyWidget->HideDialogueBubble_UI(/*bPlayFadeOut*/false);
}

// =========================================================
// LobbyWidget registration
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::SetLobbyWidget(UMosesLobbyWidget* InWidget)
{
	LobbyWidget = InWidget;

	// ✅ 위젯 재생성/재등록 시 "현재 모드" 즉시 복구 (Greeting 금지)
	if (AMosesLobbyGameState* LGS = GetLobbyGameState())
	{
		const bool bDialogue = (LGS->GetGamePhase() == EGamePhase::LobbyDialogue);

		// ✅ Recover는 Greeting 없이 상태만 맞춤
		RecoverRulesView_UIOnly(bDialogue);

		// Dialogue state도 즉시 캐시/브로드캐스트
		HandleDialogueStateChanged(LGS->GetDialogueNetState());
	}
	else
	{
		// GS 없으면 일단 OFF 복구
		RecoverRulesView_UIOnly(false);
	}

	RefreshLobbyUI_FromCurrentState();

	// ✅ 위젯이 붙는 순간 하단 UI도 현재값으로 1회 반영
	ApplySessionToWidget();
}


void UMosesLobbyLocalPlayerSubsystem::RequestEnterDialogueRetry_NextTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TM = World->GetTimerManager();

	if (TM.TimerExists(EnterDialogueRetryHandle) &&
		(TM.IsTimerActive(EnterDialogueRetryHandle) || TM.IsTimerPending(EnterDialogueRetryHandle)))
	{
		return;
	}

	EnterDialogueRetryHandle = TM.SetTimerForNextTick(this, &UMosesLobbyLocalPlayerSubsystem::RequestEnterLobbyDialogue);
}

void UMosesLobbyLocalPlayerSubsystem::ClearEnterDialogueRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnterDialogueRetryHandle);
	}
}

// =========================================================
// Retry control (Preview refresh - 중복 예약 방지)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestPreviewRefreshRetry_NextTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TM = World->GetTimerManager();

	// ✅ 이미 예약돼 있으면 또 예약하지 않음
	if (TM.TimerExists(PreviewRefreshRetryHandle) &&
		(TM.IsTimerActive(PreviewRefreshRetryHandle) || TM.IsTimerPending(PreviewRefreshRetryHandle)))
	{
		return;
	}

	PreviewRefreshRetryHandle = TM.SetTimerForNextTick(this, &UMosesLobbyLocalPlayerSubsystem::RefreshLobbyPreviewCharacter_LocalOnly);
}

void UMosesLobbyLocalPlayerSubsystem::ClearPreviewRefreshRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PreviewRefreshRetryHandle);
}

// =========================================================
// UI -> Server Phase Request
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestEnterLobbyDialogue()
{
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	AMosesPlayerState* PS = PC ? PC->GetPlayerState<AMosesPlayerState>() : nullptr;

	const bool bConnReady = (PC && PC->GetNetConnection());
	const bool bPSReady = (PS != nullptr);
	const bool bRoomValid = (PS && PS->GetRoomId().IsValid()); // 로그용만 유지

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[DOD][PRECHECK] PC=%s Local=%d Conn=%d PS=%s RoomValid=%d RoomId=%s"),
		*GetNameSafe(PC),
		PC ? (PC->IsLocalController() ? 1 : 0) : 0,
		bConnReady ? 1 : 0,
		*GetNameSafe(PS),
		bRoomValid ? 1 : 0,
		PS ? *PS->GetRoomId().ToString() : TEXT("None"));

	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// Conn / PS 준비 안 된 경우에만 retry
	if (!bConnReady || !bPSReady)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][UI->SV] EnterLobbyDialogue WAIT (Conn/PS not ready) -> retry"));
		RequestEnterDialogueRetry_NextTick();
		return;
	}

	// ✅ 방 체크 제거: 로비 기본 상태(방 밖)에서도 서버 대화 진입 허용
	ClearEnterDialogueRetry();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][UI->SV] RequestEnterLobbyDialogue SEND (Room independent)"));
	PC->Server_RequestEnterLobbyDialogue(ELobbyDialogueEntryType::RulesView);
}

void UMosesLobbyLocalPlayerSubsystem::RequestExitLobbyDialogue()
{
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][UI->SV] RequestExitLobbyDialogue"));
	PC->Server_RequestExitLobbyDialogue();
}


// =========================================================
// Phase / Dialogue state handlers
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::HandlePhaseChanged(EGamePhase NewPhase)
{
	UE_LOG(LogMosesSpawn, Log, TEXT("[DOD][LPS] HandlePhaseChanged Phase=%d"), (int32)NewPhase);

	// "연출은 Phase 기반으로만 실행"
	if (NewPhase == EGamePhase::LobbyDialogue)
	{
		EnterRulesView_UIOnly();
	}
	else
	{
		ExitRulesView_UIOnly();
	}
}

void UMosesLobbyLocalPlayerSubsystem::HandleGameStateDialogueChanged(const FDialogueNetState& NewState)
{
	LastDialogueNetState = NewState;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LPS] DialogueChanged Line=%d Flow=%d Sub=%d Seq=%d"),
		NewState.LineIndex, (int32)NewState.FlowState, (int32)NewState.SubState, NewState.LastCommandSeq);

	LobbyDialogueStateChanged.Broadcast(NewState);
}

void UMosesLobbyLocalPlayerSubsystem::HandleDialogueStateChanged(const FDialogueNetState& NewState)
{
	NotifyDialogueStateChanged(NewState);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LPS->UI] DialogueState Broadcast(Alt) Line=%d Flow=%d Sub=%d Seq=%d"),
		NewState.LineIndex, (int32)NewState.FlowState, (int32)NewState.SubState, NewState.LastCommandSeq);
}

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
		// ✅ PS 준비 전이면 다음 틱 재시도
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UMosesLobbyLocalPlayerSubsystem::TrySendPendingLobbyNickname_Local);
		}
		return;
	}

	if (PendingLobbyNickname_Local.IsEmpty())
	{
		bPendingLobbyNicknameSend_Local = false;
		return;
	}

	// ✅ Standalone/ListenServer(권한 있음) => 서버 RPC 말고 직접 세팅
	if (PC->HasAuthority())
	{
		PS->ServerSetPlayerNickName(PendingLobbyNickname_Local);
		PS->ServerSetLoggedIn(true);

		bPendingLobbyNicknameSend_Local = false;

		UE_LOG(LogMosesSpawn, Log, TEXT("[Nick][LPS] Applied locally (Authority) Nick='%s' PS=%s"),
			*PendingLobbyNickname_Local, *GetNameSafe(PS));

		// ✅ 핵심: PS에 값 쓴 직후 UI 리프레시 한번 강제
		RefreshLobbyUI_FromCurrentState();
		return;
	}

	// ✅ 진짜 클라이언트(서버에 붙어있는 상태)일 때만 RPC
	if (!PC->GetNetConnection())
	{
		// 커넥션 아직 없으면 다음 틱 재시도
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UMosesLobbyLocalPlayerSubsystem::TrySendPendingLobbyNickname_Local);
		}
		return;
	}

	PC->Server_SetLobbyNickname(PendingLobbyNickname_Local);
	bPendingLobbyNicknameSend_Local = false;

	UE_LOG(LogMosesSpawn, Log, TEXT("[Nick][LPS] Sent Server_SetLobbyNickname '%s' PC=%s PS=%s"),
		*PendingLobbyNickname_Local, *GetNameSafe(PC), *GetNameSafe(PS));
}

void UMosesLobbyLocalPlayerSubsystem::NotifyDialogueStateChanged(const FDialogueNetState& NewState)
{
	// =========================================================
	// "답변 시작" 트리거(임시 규칙)
	// - RulesView 중 LineIndex가 바뀌면 새로운 답변 라인이 시작된 것으로 간주
	// - 그 시점에 제스처를 1회 재생
	// =========================================================
	if (bInRulesView)
	{
		if (CachedLastGestureLineIndex != NewState.LineIndex)
		{
			CachedLastGestureLineIndex = NewState.LineIndex;
			PlayRulesMetaHumanGesture_LocalOnly();
		}
	}

	// late join / widget 재생성 복구용 캐시
	LastDialogueNetState = NewState;

	// LobbyWidget 구독자에게 브로드캐스트
	LobbyDialogueStateChanged.Broadcast(NewState);
}

void UMosesLobbyLocalPlayerSubsystem::RequestSetLobbyNickname_LocalOnly(const FString& Nick)
{
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// ✅ “로그인 버튼을 눌렀다”는 의도 플래그
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
// Dialogue command submit
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::SubmitCommandText(const FString& Text)
{
	const FDialogueDetectResult R = UMosesDialogueCommandDetector::DetectIntent(Text, CommandLexicon);

	UE_LOG(LogTemp, Log, TEXT("[CmdDetect] \"%s\" -> %d (norm=\"%s\" kw=\"%s\")"),
		*Text, (int32)R.Type, *R.NormalizedText, *R.MatchedKeyword);

	if (R.Type == EDialogueCommandType::None)
	{
		return;
	}

	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CmdDetect] no local PC"));
		return;
	}

	const uint16 Seq = NextClientCommandSeq++;
	PC->Server_SubmitDialogueCommand(R.Type, Seq);
}

void AMosesLobbyGameState::OnRep_ChatHistory()
{
	// 너 기존 UI 갱신 파이프 재사용(로컬플레이어 Subsystem으로 전달)
	NotifyRoomStateChanged_LocalPlayers();
}

void UMosesLobbyLocalPlayerSubsystem::SetAnswer(int32 AnswerIndex, const FText& AnswerText)
{
	// =========================================================
	// 테스트 & Day5 확장용 단일 진실 함수
	// - 중앙 자막(UI) 업데이트 + 제스처 1회 트리거
	// - AnswerIndex는 지금은 로깅/추후 확장용
	// =========================================================
	if (LobbyWidget)
	{
		LobbyWidget->ShowDialogueBubble_UI(/*bPlayFadeIn*/true);
		LobbyWidget->SetSubtitleVisibility(true);
		LobbyWidget->SetDialogueBubbleText_UI(AnswerText);
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[Day3][SetAnswer] Index=%d Text='%s'"),
		AnswerIndex, *AnswerText.ToString());

	PlayRulesMetaHumanGesture_LocalOnly();
}

AActor* UMosesLobbyLocalPlayerSubsystem::GetOrFindRulesMetaHumanActor_LocalOnly()
{
	if (CachedRulesMetaHumanActor.IsValid())
	{
		return CachedRulesMetaHumanActor.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(World, RulesMetaHumanActorTag, Found);

	if (Found.Num() > 0)
	{
		CachedRulesMetaHumanActor = Found[0];

		UE_LOG(LogMosesSpawn, Log, TEXT("[Day3][RulesMetaHuman] Found Actor=%s Tag=%s"),
			*GetNameSafe(CachedRulesMetaHumanActor.Get()),
			*RulesMetaHumanActorTag.ToString());

		return CachedRulesMetaHumanActor.Get();
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[Day3][RulesMetaHuman] Not found. Tag=%s (Place actor in level and set ActorTag)"),
		*RulesMetaHumanActorTag.ToString());

	return nullptr;
}

void UMosesLobbyLocalPlayerSubsystem::PlayRulesMetaHumanGesture_LocalOnly()
{
	AActor* Actor = GetOrFindRulesMetaHumanActor_LocalOnly();
	if (!Actor)
	{
		return;
	}

	UMosesGestureDeckComponent* Deck = Actor->FindComponentByClass<UMosesGestureDeckComponent>();
	if (!Deck)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[Day3][RulesMetaHuman] GestureDeckComponent missing on Actor=%s"), *GetNameSafe(Actor));
		return;
	}

	Deck->PlayNextGesture();
}

void UMosesLobbyLocalPlayerSubsystem::StopRulesMetaHumanGesture_LocalOnly()
{
	AActor* Actor = CachedRulesMetaHumanActor.Get();
	if (!Actor)
	{
		return;
	}

	UMosesGestureDeckComponent* Deck = Actor->FindComponentByClass<UMosesGestureDeckComponent>();
	if (!Deck)
	{
		return;
	}

	Deck->StopGesture(0.2f);
}
