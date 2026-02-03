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
}

void AMosesMatchGameState::ServerStartMatchTimer(int32 TotalSeconds)
{
	// [MOD] “진짜 호출되는지”부터 강제 로그
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

	// [MOD] PushModel: Replicated 변경 Dirty 마킹 필수
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, RemainingSeconds, this);
	ForceNetUpdate();

	// 서버에서도 UI 갱신을 위해 RepNotify를 직접 호출(Delegate 목적)
	OnRep_RemainingSeconds();

	// ---------------------------------------------------------------------
	// Announcement tick
	// ---------------------------------------------------------------------
	if (AnnouncementState.bActive)
	{
		AnnouncementState.RemainingSeconds = FMath::Max(0, AnnouncementState.RemainingSeconds - 1);

		if (AnnouncementState.bCountdown)
		{
			AnnouncementState.Text = FText::Format(
				FText::FromString(TEXT("{0} {1}")),
				ServerAnnouncePrefixText,
				FText::AsNumber(AnnouncementState.RemainingSeconds));
		}

		// [MOD] PushModel: struct도 Dirty 마킹
		MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
		ForceNetUpdate();

		OnRep_AnnouncementState();

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

	// [MOD] PushModel dirty
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

	// [MOD] PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, MatchPhase, this);
	ForceNetUpdate();

	OnRep_MatchPhase();

	UE_LOG(LogMosesPhase, Log, TEXT("[PHASE][SV] MatchPhase=%s"), *UEnum::GetValueAsString(MatchPhase));
}

void AMosesMatchGameState::ServerStartAnnouncementText(const FText& InText, int32 DurationSeconds)
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

	// [MOD] PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
	ForceNetUpdate();

	UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] Start Text Duration=%d"), DurationSeconds);

	OnRep_AnnouncementState();
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

	// [MOD] PushModel dirty
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

	// [MOD] PushModel dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AMosesMatchGameState, AnnouncementState, this);
	ForceNetUpdate();

	UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] Stop"));

	OnRep_AnnouncementState();
}

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
