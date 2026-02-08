#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h"
#include "TimerManager.h"

#include "Net/Core/PushModel/PushModel.h"

AMosesMatchGameState::AMosesMatchGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
}

void AMosesMatchGameState::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMosesExp, Warning,
		TEXT("[MatchGS] BeginPlay World=%s NetMode=%d ExpComp=%s"),
		*GetNameSafe(GetWorld()),
		(int32)GetNetMode(),
		*GetNameSafe(GetExperienceManagerComponent()));
}

void AMosesMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesMatchGameState, RemainingSeconds);
	DOREPLIFETIME(AMosesMatchGameState, MatchPhase);
	DOREPLIFETIME(AMosesMatchGameState, AnnouncementState);
	DOREPLIFETIME(AMosesMatchGameState, ResultState);
}

// ============================================================================
// Match Timer
// ============================================================================

void AMosesMatchGameState::ServerStartMatchTimer(int32 TotalSeconds)
{
	// “진짜 호출되는지”부터 강제 로그
	UE_LOG(LogMosesPhase, Warning, TEXT("[MatchTime][SV] ServerStartMatchTimer CALLED Total=%d HasAuth=%d World=%s"),
		TotalSeconds,
		HasAuthority() ? 1 : 0,
		*GetNameSafe(GetWorld()));

	if (!HasAuthority())
	{
		return;
	}

	TotalSeconds = FMath::Max(0, TotalSeconds);

	// 초기 값 세팅(Rep + Delegate)
	ServerSetRemainingSeconds(TotalSeconds);

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &ThisClass::ServerTick_1s, 1.0f, true);

	UE_LOG(LogMosesExp, Warning, TEXT("[MatchTime][SV] Start Total=%d"), TotalSeconds);
}

void AMosesMatchGameState::ServerStopMatchTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
}

void AMosesMatchGameState::ServerTick_1s()
{
	check(HasAuthority());

	// ---------------------------------------------------------------------
	// Match RemainingSeconds
	// ---------------------------------------------------------------------
	RemainingSeconds = FMath::Max(0, RemainingSeconds - 1);

	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, RemainingSeconds, this);
	ForceNetUpdate();

	// [MOD] 서버 로컬 UI 갱신은 RepNotify 직접 호출 금지 -> Delegate 직접 방송
	BroadcastMatchTimeLocal_Server();

	// ---------------------------------------------------------------------
	// [MOD] Announcement tick (IMPORTANT FIX)
	// ---------------------------------------------------------------------
	// ✅ bAnnouncementExternallyDriven == true 인 동안에는
	//    여기서 RemainingSeconds를 감소시키거나 Stop하면 안 된다.
	//    (RespawnManager가 매초 값을 넣는데 여기서도 감소/Stop하면 깜빡임)
	if (AnnouncementState.bActive && !bAnnouncementExternallyDriven)
	{
		AnnouncementState.RemainingSeconds = FMath::Max(0, AnnouncementState.RemainingSeconds - 1);

		if (AnnouncementState.bCountdown)
		{
			AnnouncementState.Text = FText::Format(
				FText::FromString(TEXT("{0} {1}")),
				ServerAnnouncePrefixText,
				FText::AsNumber(AnnouncementState.RemainingSeconds));
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
		ForceNetUpdate();

		BroadcastAnnouncementLocal_Server();

		if (AnnouncementState.RemainingSeconds <= 0)
		{
			ServerStopAnnouncement();
		}
	}

	// ---------------------------------------------------------------------
	// Finished
	// ---------------------------------------------------------------------
	if (RemainingSeconds <= 0)
	{
		ServerStopMatchTimer();
		UE_LOG(LogMosesExp, Warning, TEXT("[MatchTime][SV] Finished"));
	}
}

void AMosesMatchGameState::ServerSetRemainingSeconds(int32 NewSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	NewSeconds = FMath::Max(0, NewSeconds);

	if (RemainingSeconds == NewSeconds)
	{
		return;
	}

	RemainingSeconds = NewSeconds;

	// PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, RemainingSeconds, this);
	ForceNetUpdate();

	OnRep_RemainingSeconds();
}

void AMosesMatchGameState::ServerSetMatchPhase(EMosesMatchPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (MatchPhase == NewPhase)
	{
		return;
	}

	MatchPhase = NewPhase;

	// PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, MatchPhase, this);
	ForceNetUpdate();

	OnRep_MatchPhase();

	UE_LOG(LogMosesPhase, Log, TEXT("[PHASE][SV] MatchPhase=%s"), *UEnum::GetValueAsString(MatchPhase));
}

// ============================================================================
// Announcement
// ============================================================================

void AMosesMatchGameState::ServerSetCountdownAnnouncement_External(int32 RemainingSec)
{
	if (!HasAuthority())
	{
		return;
	}

	RemainingSec = FMath::Max(0, RemainingSec);

	// [MOD] 외부 구동 카운트다운 시작/유지 플래그 ON
	bAnnouncementExternallyDriven = true;

	const FText Text = FText::Format(
		FText::FromString(TEXT("좀비 리스폰까지 {0}초")),
		FText::AsNumber(RemainingSec));

	AnnouncementState.bActive = true;
	AnnouncementState.bCountdown = true;
	AnnouncementState.Text = Text;
	AnnouncementState.RemainingSeconds = RemainingSec;

	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
	ForceNetUpdate();

	// 서버 로컬 UI
	BroadcastAnnouncementLocal_Server();

	UE_LOG(LogMosesPhase, Verbose, TEXT("[ANN][SV] ExternalCountdown Update Remaining=%d"), RemainingSec);

	// [MOD] RemainingSec==0이 되는 순간 “여기서” Stop하는 게 안전
	// (내부 Tick과 충돌 X)
	if (RemainingSec <= 0)
	{
		ServerStopAnnouncement();
	}
}

void AMosesMatchGameState::ServerStartAnnouncementText(
	const FText& InText,
	int32 DurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	DurationSeconds = FMath::Max(1, DurationSeconds);

	AnnouncementState.bActive = true;
	AnnouncementState.bCountdown = false;
	AnnouncementState.Text = InText;
	AnnouncementState.RemainingSeconds = DurationSeconds;

	MARK_PROPERTY_DIRTY_FROM_NAME(
		AMosesMatchGameState,
		AnnouncementState,
		this);

	ForceNetUpdate();

	UE_LOG(LogMosesPhase, Warning,
		TEXT("[ANN][SV] Start Text Duration=%d"),
		DurationSeconds);
}

void AMosesMatchGameState::ServerStartAnnouncementCountdown(const FText& PrefixText, int32 CountdownFromSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	CountdownFromSeconds = FMath::Max(1, CountdownFromSeconds);

	ServerAnnouncePrefixText = PrefixText;

	AnnouncementState.bActive = true;
	AnnouncementState.bCountdown = true;
	AnnouncementState.RemainingSeconds = CountdownFromSeconds;
	AnnouncementState.Text = FText::Format(
		FText::FromString(TEXT("{0} {1}")),
		ServerAnnouncePrefixText,
		FText::AsNumber(AnnouncementState.RemainingSeconds));

	// PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
	ForceNetUpdate();

	UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] Start Countdown From=%d"), CountdownFromSeconds);

	OnRep_AnnouncementState();
}

void AMosesMatchGameState::ServerStopAnnouncement()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!AnnouncementState.bActive)
	{
		return;
	}

	AnnouncementState = FMosesAnnouncementState();

	// PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
	ForceNetUpdate();

	UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] Stop"));

	OnRep_AnnouncementState();
}

// ============================================================================
// [MOD] Result
// ============================================================================

void AMosesMatchGameState::ServerSetResultState(const FMosesMatchResultState& NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	ResultState = NewState;

	// PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, ResultState, this);
	ForceNetUpdate();

	UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][SV] bIsResult=%d bDraw=%d WinnerPid=%s Nick=%s Reason=%s"),
		ResultState.bIsResult ? 1 : 0,
		ResultState.bIsDraw ? 1 : 0,
		*ResultState.WinnerPersistentId,
		*ResultState.WinnerNickname,
		*ResultState.ResultReason);

	OnRep_ResultState();
}

// ============================================================================
// RepNotifies -> Delegates
// ============================================================================

void AMosesMatchGameState::OnRep_RemainingSeconds()
{
	OnMatchTimeChanged.Broadcast(RemainingSeconds);
}

void AMosesMatchGameState::OnRep_MatchPhase()
{
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void AMosesMatchGameState::OnRep_AnnouncementState()
{
	OnAnnouncementChanged.Broadcast(AnnouncementState);
}

void AMosesMatchGameState::OnRep_ResultState()
{
	OnResultStateChanged.Broadcast(ResultState);
}

// ============================================================================
// Server-local delegate broadcast (NO OnRep direct call)
// ============================================================================

void AMosesMatchGameState::BroadcastMatchTimeLocal_Server()
{
	OnMatchTimeChanged.Broadcast(RemainingSeconds);
}

void AMosesMatchGameState::BroadcastMatchPhaseLocal_Server()
{
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void AMosesMatchGameState::BroadcastAnnouncementLocal_Server()
{
	OnAnnouncementChanged.Broadcast(AnnouncementState);
}

void AMosesMatchGameState::BroadcastResultLocal_Server()
{
	OnResultStateChanged.Broadcast(ResultState);
}

// ============================================================================
// Compatibility Wrappers
// ============================================================================

void AMosesMatchGameState::ServerPushAnnouncement(const FText& Msg, float DurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Seconds = FMath::Max(1, FMath::CeilToInt(DurationSeconds));

	UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] Push Text Seconds=%d"), Seconds);

	ServerStartAnnouncementText(Msg, Seconds);
}

void AMosesMatchGameState::ServerPushAnnouncement(const FString& Msg, float DurationSeconds)
{
	ServerPushAnnouncement(FText::FromString(Msg), DurationSeconds);
}

void AMosesMatchGameState::ServerPushKillFeed(const FText& Msg, bool bHeadshot, USoundBase* HeadshotSound)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesKillFeed, Warning,
		TEXT("[KILLFEED][SV] Headshot=%d Sound=%s"),
		bHeadshot ? 1 : 0,
		*GetNameSafe(HeadshotSound));

	// TODO:
	// KillFeed 전용 RepState(Array/Queue) 구현 예정
	// 현재는 Announcement로 대체 출력
	ServerStartAnnouncementText(Msg, 2);
}

void AMosesMatchGameState::ServerPushKillFeed(const FText& Msg)
{
	ServerPushKillFeed(Msg, false, nullptr);
}

void AMosesMatchGameState::ServerPushKillFeed(const FString& Msg)
{
	ServerPushKillFeed(FText::FromString(Msg));
}
