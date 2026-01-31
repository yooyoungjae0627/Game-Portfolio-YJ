// ============================================================================
// MosesMatchHUD.cpp (FULL)
// - [MOD] PhaseText: Warmup -> "워밍업"(흰), Combat -> "매치"(빨강), Result -> "결과"(흰)
// - Tick/Binding 금지: Delegate 이벤트에서만 SetText/SetColor
// - [MOD] 클라2 간헐 타이머 미갱신: GameState 바인딩 타이밍 이슈 → Timer 재시도
// - [MOD] Phase rollback 방지 (Combat인데 Warmup이 늦게 와서 덮어쓰는 문제)
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesMatchHUD.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"

UMosesMatchHUD::UMosesMatchHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesMatchHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Init MatchHUD=%s OwningPlayer=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwningPlayer()));

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] PhaseText Bind=%s (Check WBP name == PhaseText)"),
		*GetNameSafe(PhaseText));

	BindToPlayerState();
	BindToGameState_Match();

	// [MOD] 초기 표시 (바인딩 실패해도 일단 기본값 노출)
	RefreshInitial();

	// [MOD] 클라2 타이밍 이슈 방지: 바인딩 재시도
	ScheduleBindRetry();
}

void UMosesMatchHUD::NativeDestruct()
{
	// [MOD] Bind 재시도 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
	}

	if (AMosesPlayerState* PS = CachedPlayerState.Get())
	{
		PS->OnHealthChanged.RemoveAll(this);
		PS->OnShieldChanged.RemoveAll(this);
		PS->OnScoreChanged.RemoveAll(this);
		PS->OnDeathsChanged.RemoveAll(this);
		PS->OnAmmoChanged.RemoveAll(this);
		PS->OnGrenadeChanged.RemoveAll(this);

		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Unbound PlayerState delegates PS=%s"), *GetNameSafe(PS));
	}

	if (AMosesMatchGameState* GS = CachedMatchGameState.Get())
	{
		GS->OnMatchTimeChanged.RemoveAll(this);
		GS->OnMatchPhaseChanged.RemoveAll(this);
		GS->OnAnnouncementChanged.RemoveAll(this);

		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Unbound MatchGameState delegates GS=%s"), *GetNameSafe(GS));
	}

	CachedPlayerState.Reset();
	CachedMatchGameState.Reset();

	Super::NativeDestruct();
}

// ============================================================================
// Bind Retry (Tick 금지 → Timer 기반)
// ============================================================================

bool UMosesMatchHUD::IsBoundToPlayerState() const
{
	return CachedPlayerState.IsValid();
}

bool UMosesMatchHUD::IsBoundToMatchGameState() const
{
	return CachedMatchGameState.IsValid();
}

void UMosesMatchHUD::ScheduleBindRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (BindRetryHandle.IsValid())
	{
		return;
	}

	BindRetryCount = 0;

	World->GetTimerManager().SetTimer(
		BindRetryHandle,
		this,
		&ThisClass::TryBindRetry,
		BindRetryInterval,
		true);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry START Interval=%.2f"), BindRetryInterval);
}

void UMosesMatchHUD::TryBindRetry()
{
	BindRetryCount++;

	// 이미 다 물렸으면 종료
	if (IsBoundToPlayerState() && IsBoundToMatchGameState())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryHandle);
		}

		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry DONE"));
		return;
	}

	// 아직 덜 물렸으면 계속 시도
	if (!IsBoundToPlayerState())
	{
		BindToPlayerState();
	}

	if (!IsBoundToMatchGameState())
	{
		BindToGameState_Match();
	}

	// 너무 오래 걸리면 포기 (증거 로그)
	if (BindRetryCount >= BindRetryMaxCount)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryHandle);
		}

		UE_LOG(LogMosesHUD, Error, TEXT("[HUD][CL] BindRetry GIVEUP (PlayerState=%d GameState=%d)"),
			IsBoundToPlayerState() ? 1 : 0,
			IsBoundToMatchGameState() ? 1 : 0);
	}
}

// ============================================================================
// Bind
// ============================================================================

void UMosesMatchHUD::BindToPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] BindToPlayerState: No PC"));
		return;
	}

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] BindToPlayerState: No PS PC=%s"), *GetNameSafe(PC));
		return;
	}

	// [MOD] 중복 바인딩 방지: 이미 같은 PS면 스킵
	if (CachedPlayerState.Get() == PS)
	{
		return;
	}

	// [MOD] 기존 PS 바인딩 해제
	if (AMosesPlayerState* OldPS = CachedPlayerState.Get())
	{
		OldPS->OnHealthChanged.RemoveAll(this);
		OldPS->OnShieldChanged.RemoveAll(this);
		OldPS->OnScoreChanged.RemoveAll(this);
		OldPS->OnDeathsChanged.RemoveAll(this);
		OldPS->OnAmmoChanged.RemoveAll(this);
		OldPS->OnGrenadeChanged.RemoveAll(this);
	}

	CachedPlayerState = PS;

	PS->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	PS->OnShieldChanged.AddUObject(this, &ThisClass::HandleShieldChanged);
	PS->OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	PS->OnDeathsChanged.AddUObject(this, &ThisClass::HandleDeathsChanged);
	PS->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged);
	PS->OnGrenadeChanged.AddUObject(this, &ThisClass::HandleGrenadeChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound PlayerState delegates PS=%s"), *GetNameSafe(PS));
}

void UMosesMatchHUD::BindToGameState_Match()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] BindToGameState: No World"));
		return;
	}

	AMosesMatchGameState* GS = World->GetGameState<AMosesMatchGameState>();
	if (!GS)
	{
		UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] No AMosesMatchGameState yet World=%s"), *GetNameSafe(World));
		return;
	}

	// [MOD] 중복 바인딩 방지: 이미 같은 GS면 스킵
	if (CachedMatchGameState.Get() == GS)
	{
		return;
	}

	// [MOD] 기존 GS 바인딩 해제
	if (AMosesMatchGameState* OldGS = CachedMatchGameState.Get())
	{
		OldGS->OnMatchTimeChanged.RemoveAll(this);
		OldGS->OnMatchPhaseChanged.RemoveAll(this);
		OldGS->OnAnnouncementChanged.RemoveAll(this);
	}

	CachedMatchGameState = GS;

	GS->OnMatchTimeChanged.AddUObject(this, &ThisClass::HandleMatchTimeChanged);
	GS->OnMatchPhaseChanged.AddUObject(this, &ThisClass::HandleMatchPhaseChanged);
	GS->OnAnnouncementChanged.AddUObject(this, &ThisClass::HandleAnnouncementChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound MatchGameState delegates GS=%s"), *GetNameSafe(GS));

	// Tick/Binding 없이 현재 상태를 즉시 반영
	HandleMatchTimeChanged(GS->GetRemainingSeconds());
	HandleMatchPhaseChanged(GS->GetMatchPhase());
	HandleAnnouncementChanged(GS->GetAnnouncementState());
}

void UMosesMatchHUD::RefreshInitial()
{
	HandleMatchTimeChanged(0);
	HandleScoreChanged(0);
	HandleDeathsChanged(0);
	HandleAmmoChanged(0, 0);
	HandleGrenadeChanged(0);
	HandleHealthChanged(100.f, 100.f);
	HandleShieldChanged(100.f, 100.f);

	// [MOD] 초기 표시
	LastAppliedPhase = EMosesMatchPhase::WaitingForPlayers;
	HandleMatchPhaseChanged(EMosesMatchPhase::Warmup);
}

// ============================================================================
// PlayerState Handlers
// ============================================================================

void UMosesMatchHUD::HandleHealthChanged(float Current, float Max)
{
	if (HealthBar)
	{
		const float Pct = (Max > 0.f) ? (Current / Max) : 0.f;
		HealthBar->SetPercent(Pct);
	}

	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f/%.0f"), Current, Max)));
	}
}

void UMosesMatchHUD::HandleShieldChanged(float Current, float Max)
{
	if (ShieldBar)
	{
		const float Pct = (Max > 0.f) ? (Current / Max) : 0.f;
		ShieldBar->SetPercent(Pct);
	}

	if (ShieldText)
	{
		ShieldText->SetText(FText::FromString(FString::Printf(TEXT("%.0f/%.0f"), Current, Max)));
	}
}

void UMosesMatchHUD::HandleScoreChanged(int32 NewScore)
{
	if (ScoreAmount)
	{
		ScoreAmount->SetText(FText::AsNumber(NewScore));
	}
}

void UMosesMatchHUD::HandleDeathsChanged(int32 NewDeaths)
{
	if (DefeatsAmount)
	{
		DefeatsAmount->SetText(FText::AsNumber(NewDeaths));
	}
}

void UMosesMatchHUD::HandleAmmoChanged(int32 Mag, int32 Reserve)
{
	if (WeaponAmmoAmount)
	{
		WeaponAmmoAmount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, Reserve)));
	}
}

void UMosesMatchHUD::HandleGrenadeChanged(int32 Grenade)
{
	if (GrenadeAmount)
	{
		GrenadeAmount->SetText(FText::AsNumber(Grenade));
	}
}

// ============================================================================
// MatchGameState Handlers
// ============================================================================

void UMosesMatchHUD::HandleMatchTimeChanged(int32 RemainingSeconds)
{
	if (MatchCountdownText)
	{
		MatchCountdownText->SetText(FText::FromString(ToMMSS(RemainingSeconds)));
	}
}

void UMosesMatchHUD::HandleMatchPhaseChanged(EMosesMatchPhase NewPhase)
{
	const int32 OldP = GetPhasePriority(LastAppliedPhase);
	const int32 NewP = GetPhasePriority(NewPhase);

	// [MOD] 역행(Combat인데 Warmup이 늦게 도착 등)은 무시
	if (NewP < OldP)
	{
		UE_LOG(LogMosesPhase, Warning,
			TEXT("[HUD][CL] PhaseChanged IGNORE (rollback) New=%s Old=%s"),
			*UEnum::GetValueAsString(NewPhase),
			*UEnum::GetValueAsString(LastAppliedPhase));
		return;
	}

	LastAppliedPhase = NewPhase;

	UE_LOG(LogMosesPhase, Warning, TEXT("[HUD][CL] PhaseChanged APPLY New=%s"),
		*UEnum::GetValueAsString(NewPhase));

	if (PhaseText)
	{
		PhaseText->SetText(GetPhaseText_KR(NewPhase));
		PhaseText->SetColorAndOpacity(GetPhaseColor(NewPhase)); // [MOD] 색상 반영
	}
	else
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] PhaseText is NULL (Check WBP name == 'PhaseText')"));
	}
}

void UMosesMatchHUD::HandleAnnouncementChanged(const FMosesAnnouncementState& State)
{
	if (AnnouncementWidget)
	{
		AnnouncementWidget->UpdateAnnouncement(State);
	}
}

// ============================================================================
// Util
// ============================================================================

FString UMosesMatchHUD::ToMMSS(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 MM = Clamped / 60;
	const int32 SS = Clamped % 60;
	return FString::Printf(TEXT("%02d:%02d"), MM, SS);
}

FText UMosesMatchHUD::GetPhaseText_KR(EMosesMatchPhase Phase)
{
	switch (Phase)
	{
	case EMosesMatchPhase::Warmup: return FText::FromString(TEXT("워밍업"));
	case EMosesMatchPhase::Combat: return FText::FromString(TEXT("매치"));
	case EMosesMatchPhase::Result: return FText::FromString(TEXT("결과"));
	default:                       return FText::FromString(TEXT("알 수 없음"));
	}
}

FSlateColor UMosesMatchHUD::GetPhaseColor(EMosesMatchPhase Phase)
{
	// [MOD] Warmup/Result = White, Combat = Red
	switch (Phase)
	{
	case EMosesMatchPhase::Combat:
		return FSlateColor(FLinearColor(1.f, 0.f, 0.f, 1.f)); // 빨강
	case EMosesMatchPhase::Warmup:
	case EMosesMatchPhase::Result:
	default:
		return FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)); // 흰색
	}
}

int32 UMosesMatchHUD::GetPhasePriority(EMosesMatchPhase Phase)
{
	switch (Phase)
	{
	case EMosesMatchPhase::WaitingForPlayers: return 0;
	case EMosesMatchPhase::Warmup:           return 1;
	case EMosesMatchPhase::Combat:           return 2;
	case EMosesMatchPhase::Result:           return 3;
	default:                                 return -1;
	}
}
