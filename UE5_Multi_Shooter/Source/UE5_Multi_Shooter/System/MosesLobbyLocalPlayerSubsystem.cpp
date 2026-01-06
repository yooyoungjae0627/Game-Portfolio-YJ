#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyWidget.h"

#include "UE5_Multi_Shooter/Lobby/LobbyPreviewActor.h"

#include "Camera/CameraActor.h"

#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

// ✅ 추가: PS NULL 재시도(타이머)용
#include "TimerManager.h"

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
}

void UMosesLobbyLocalPlayerSubsystem::Deinitialize()
{
	// 개발자 주석:
	// - 로컬플레이어가 내려갈 때 UI도 내려준다.
	DeactivateLobbyUI();

	ExitRulesView_UIOnly();

	CachedLobbyPreviewActor = nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Deinitialize LP=%s"), *GetNameSafe(GetLocalPlayer()));

	Super::Deinitialize();
}

void UMosesLobbyLocalPlayerSubsystem::EnterRulesView()
{
	// 1) 상태 변경 (UI 숨김/표시)
	SetLobbyViewMode(ELobbyViewMode::RulesView);

	// 2) 카메라 전환
	SetViewTargetToCameraTag(TEXT("LobbyRulesCamera"), /*BlendTime*/0.35f);
}

void UMosesLobbyLocalPlayerSubsystem::ExitRulesView()
{
	// 1) 상태 변경 (UI 원복)
	SetLobbyViewMode(ELobbyViewMode::Default);

	// 2) 카메라 원복
	SetViewTargetToCameraTag(TEXT("LobbyPreviewCamera"), /*BlendTime*/0.35f);
}

// =========================================================
// UI control
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	// ✅ 추가: 멀티 PIE에서 "이 Subsystem이 어느 LP에 붙었는지" 즉시 증명 로그
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

	// 개발자 주석:
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
		// 개발자 주석:
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

	// 개발자 주석:
	// - CreateWidget의 OwningPlayer가 없으면 GetOwningLocalPlayer가 NULL 될 수 있다.
	// - LocalPlayerSubsystem에서 만들면 "LP의 PlayerController"를 OwningPlayer로 넣는 게 안전하다.
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No MosesPC)"));
		return;
	}

	// ✅ 추가: 멀티 PIE에서 PC0 고정/잘못된 PC를 초기에 차단
	// - "로컬 컨트롤러"가 아니면 이 LP의 UI를 만들면 안 된다.
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

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI OK Widget=%s InViewport=%d OwningPC=%s"),
		*GetNameSafe(LobbyWidget),
		LobbyWidget->IsInViewport() ? 1 : 0,
		*GetNameSafe(PC));

	// 개발자 주석:
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
// Notify
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::NotifyPlayerStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyPlayerStateChanged"));

	// 개발자 주석:
	// - 1) Widget이 UI를 갱신할 수 있도록 신호
	LobbyPlayerStateChangedEvent.Broadcast();

	// 개발자 주석:
	// - 2) SelectedCharacterId 변경이 오면 프리뷰도 즉시 갱신
	RefreshLobbyPreviewCharacter_LocalOnly();
}

void UMosesLobbyLocalPlayerSubsystem::NotifyRoomStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyRoomStateChanged"));
	LobbyRoomStateChangedEvent.Broadcast();
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

	// ✅ 핵심:
	// - Travel/Join 직후에는 PC는 있어도 PS가 몇 틱 NULL일 수 있음 (정상)
	// - 이때 "SetTimerForNextTick 중복 예약"이 폭발하면 로그 스팸 + 흔들림이 생김
	// - 그래서 재시도는 딱 1개만 예약한다.
	if (!DebugPC || !DebugPC->IsLocalController() || !DebugPS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] WAIT (PC/PS not ready) -> retry next tick"));
		RequestPreviewRefreshRetry_NextTick();
		return;
	}

	// ✅ 성공했으면 재시도 타이머는 즉시 제거 (스팸 제거)
	ClearPreviewRefreshRetry();

	// - 여기부터는 기존 로직 유지
	const AMosesPlayerState* PS = DebugPS; // GetMosesPS_LocalOnly() 재호출 대신 위에서 잡은 걸 사용

	int32 SelectedId = PS->GetSelectedCharacterId();
	if (SelectedId != 1 && SelectedId != 2)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] SelectedId invalid (%d) -> clamp to 1"), SelectedId);
		SelectedId = 1;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] Refresh -> Apply SelectedId=%d"), SelectedId);

	// - 처음 UI 켤 때만 PS 기반으로 로컬값 초기화(디폴트=1)
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
	// ✅ 핵심 수정:
	// - 캐시가 있고 유효하면 그대로 반환
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

	return Cast<AMosesPlayerController>(LP->GetPlayerController(World)); // ★ 이게 정석
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
		// 태그가 안 달렸거나 이름이 틀리면 여기서 실패함
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
// UI Only: Rules View
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::EnterRulesView_UIOnly()
{
	// 중복 진입 방지 (왕복 10회 꼬임 방지 핵심)
	if (bInRulesView)
	{
		return;
	}

	bInRulesView = true;

	UE_LOG(LogMosesSpawn, Log, TEXT("[RulesView] Enter"));

	// 1) 카메라 전환
	ACameraActor* LobbyCam = FindCameraByTag(LobbyPreviewCameraTag);
	ACameraActor* DialogueCam = FindCameraByTag(DialogueCameraTag);

	if (!DialogueCam)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Camera] Dialogue camera not found. Tag=%s"), *DialogueCameraTag.ToString());
	}
	else
	{
		SwitchToCamera(DialogueCam, TEXT("[Camera] LobbyPreview -> DialogueCamera"));
	}

	// 2) UI 모드 전환 (패널 collapsed + dialogue ui on)
	ApplyRulesViewToWidget(true);
}

void UMosesLobbyLocalPlayerSubsystem::ExitRulesView_UIOnly()
{
	// 중복 종료 방지
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

	// 2) UI 원복
	ApplyRulesViewToWidget(false);
}

// =========================================================
// LobbyWidget registration
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::SetLobbyWidget(UMosesLobbyWidget* InWidget)
{
	LobbyWidget = InWidget;
}

// =========================================================
// Retry control (중복 예약 방지)
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
	if (TM.TimerExists(PreviewRefreshRetryHandle) && (TM.IsTimerActive(PreviewRefreshRetryHandle) || TM.IsTimerPending(PreviewRefreshRetryHandle)))
	{
		return;
	}

	// ✅ SetTimerForNextTick은 "핸들 반환" 방식만 지원한다.
	PreviewRefreshRetryHandle = TM.SetTimerForNextTick(
		this,
		&UMosesLobbyLocalPlayerSubsystem::RefreshLobbyPreviewCharacter_LocalOnly
	);
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
