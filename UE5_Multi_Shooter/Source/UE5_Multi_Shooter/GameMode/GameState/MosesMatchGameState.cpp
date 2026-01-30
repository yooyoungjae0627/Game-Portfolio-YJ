// ============================================================================
// MosesMatchGameState.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h" 

#include "TimerManager.h"

AMosesMatchGameState::AMosesMatchGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;

	// MatchLevel 진입 시 GameMode에서 GlobalPhase를 Match로 바꾸는 게 정석이지만,
	// 안전하게 생성자 기본값을 Match로 둘 수도 있다.
	// (원하면 제거하고 GameMode에서 ServerSetPhase로만 제어해도 된다.)
	if (HasAuthority())
	{
		// 생성자에서 HasAuthority는 월드 컨텍스트에 따라 애매할 수 있으므로
		// 여기서는 "기본값을 Match로 두고" GameMode가 확정하도록 맡기는 편이 안전하다.
	}
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
	if (!HasAuthority())
	{
		return;
	}

	TotalSeconds = FMath::Max(0, TotalSeconds);
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

	RemainingSeconds = FMath::Max(0, RemainingSeconds - 1);
	OnRep_RemainingSeconds();

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

		OnRep_AnnouncementState();

		if (AnnouncementState.RemainingSeconds <= 0)
		{
			ServerStopAnnouncement();
		}
	}

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
