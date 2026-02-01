#include "UE5_Multi_Shooter/UI/Match/MosesMatchHUD.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Styling/SlateColor.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"

// [DAY7/8]
#include "UE5_Multi_Shooter/UI/Match/MosesCrosshairWidget.h"
#include "UE5_Multi_Shooter/UI/Match/MosesScopeWidget.h"

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

	// 1) 즉시 바인딩 시도
	BindToPlayerState();
	BindToGameState_Match();

	// 2) 초기값(바/숫자 등) 세팅 - (Phase/Time 강제 Warmup 금지)
	RefreshInitial();

	// 3) [FIX] 아직 바인딩이 완성 안 됐으면 재시도 타이머
	StartBindRetry();

	// --------------------------------------------------------------------
	// [DAY7] Crosshair Update Loop (표시 전용)
	// - CrosshairWidget이 있으면 타이머를 돌린다.
	// --------------------------------------------------------------------
	StartCrosshairUpdate();

	// --------------------------------------------------------------------
	// [DAY8] ScopeWidget 초기 숨김
	// --------------------------------------------------------------------
	SetScopeVisible_Local(false);
}

void UMosesMatchHUD::NativeDestruct()
{
	StopBindRetry();
	StopCrosshairUpdate();

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

// --------------------------------------------------------------------
// [DAY8] Scope UI
// --------------------------------------------------------------------

void UMosesMatchHUD::SetScopeVisible_Local(bool bVisible)
{
	if (ScopeWidget)
	{
		ScopeWidget->SetScopeVisible(bVisible);
	}
}

// --------------------------------------------------------------------
// Bind / Retry
// --------------------------------------------------------------------

bool UMosesMatchHUD::IsPlayerStateBound() const
{
	return CachedPlayerState.IsValid();
}

bool UMosesMatchHUD::IsMatchGameStateBound() const
{
	return CachedMatchGameState.IsValid();
}

void UMosesMatchHUD::StartBindRetry()
{
	// 둘 다 이미 바인딩이면 필요 없음
	if (IsPlayerStateBound() && IsMatchGameStateBound())
	{
		// 그래도 혹시 이벤트 누락 복구용 스냅샷 한번 더
		ApplySnapshotFromMatchGameState();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(BindRetryHandle))
	{
		return;
	}

	BindRetryTryCount = 0;

	World->GetTimerManager().SetTimer(
		BindRetryHandle,
		this,
		&ThisClass::TryBindRetry,
		BindRetryInterval,
		true);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry START Interval=%.2f MaxTry=%d"), BindRetryInterval, BindRetryMaxTry);
}

void UMosesMatchHUD::StopBindRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(BindRetryHandle))
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry STOP"));
	}
}

void UMosesMatchHUD::TryBindRetry()
{
	BindRetryTryCount++;

	// 재시도 중이면 매번 바인딩 재시도
	if (!IsPlayerStateBound())
	{
		BindToPlayerState();
	}

	if (!IsMatchGameStateBound())
	{
		BindToGameState_Match();
	}

	// [FIX] 바인딩이 늦게 됐을 때 이벤트 누락 복구
	ApplySnapshotFromMatchGameState();

	const bool bDone = IsPlayerStateBound() && IsMatchGameStateBound();
	if (bDone)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry DONE Try=%d"), BindRetryTryCount);
		StopBindRetry();
		return;
	}

	if (BindRetryTryCount >= BindRetryMaxTry)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry GIVEUP Try=%d (PS=%d GS=%d)"),
			BindRetryTryCount,
			IsPlayerStateBound() ? 1 : 0,
			IsMatchGameStateBound() ? 1 : 0);

		StopBindRetry();
	}
}

void UMosesMatchHUD::BindToPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	// 이미 같은 PS면 중복 바인딩 방지
	if (CachedPlayerState.Get() == PS)
	{
		return;
	}

	// 기존 바인딩 제거
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
		return;
	}

	AMosesMatchGameState* GS = World->GetGameState<AMosesMatchGameState>();
	if (!GS)
	{
		return;
	}

	// 이미 같은 GS면 중복 바인딩 방지
	if (CachedMatchGameState.Get() == GS)
	{
		return;
	}

	// 기존 바인딩 제거
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

	// [FIX] 바인딩 성공 직후 스냅샷 강제 적용(이벤트 누락 대비)
	ApplySnapshotFromMatchGameState();
}

void UMosesMatchHUD::ApplySnapshotFromMatchGameState()
{
	AMosesMatchGameState* GS = CachedMatchGameState.Get();
	if (!GS)
	{
		return;
	}

	// "지금" GS에 들어있는 단일진실을 UI에 강제 적용
	HandleMatchTimeChanged(GS->GetRemainingSeconds());
	HandleMatchPhaseChanged(GS->GetMatchPhase());
	HandleAnnouncementChanged(GS->GetAnnouncementState());
}

void UMosesMatchHUD::RefreshInitial()
{
	// PlayerState 류 UI 기본값만 넣는다 (Phase/Time 강제 Warmup 금지)
	HandleScoreChanged(0);
	HandleDeathsChanged(0);
	HandleAmmoChanged(0, 0);
	HandleGrenadeChanged(0);
	HandleHealthChanged(100.f, 100.f);
	HandleShieldChanged(100.f, 100.f);

	// [FIX] Phase/Time은 GS 스냅샷을 믿는다.
	ApplySnapshotFromMatchGameState();
}

// --------------------------------------------------------------------
// [DAY7] Crosshair Update (표시 전용)
// --------------------------------------------------------------------

void UMosesMatchHUD::StartCrosshairUpdate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CrosshairWidget)
	{
		// 위젯이 없으면 루프 필요 없음
		return;
	}

	if (World->GetTimerManager().IsTimerActive(CrosshairTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CrosshairTimerHandle,
		this,
		&ThisClass::TickCrosshairUpdate,
		CrosshairUpdateInterval,
		true);
}

void UMosesMatchHUD::StopCrosshairUpdate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(CrosshairTimerHandle))
	{
		World->GetTimerManager().ClearTimer(CrosshairTimerHandle);
	}
}

float UMosesMatchHUD::CalculateCrosshairSpreadFactor_Local() const
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return 0.0f;
	}

	const float Speed2D = Pawn->GetVelocity().Size2D();

	// 표시 정책(간단): 600 기준으로 0~1 정규화
	// WeaponData 기반으로 정밀하게 하려면, CombatComponent->WeaponData(SpreadSpeedRef)로 확장 가능.
	return FMath::Clamp(Speed2D / 600.0f, 0.0f, 1.0f);
}

void UMosesMatchHUD::TickCrosshairUpdate()
{
	if (!CrosshairWidget)
	{
		return;
	}

	const float SpreadFactor = CalculateCrosshairSpreadFactor_Local();
	CrosshairWidget->SetSpreadFactor(SpreadFactor);

	// 로그 스팸 방지
	if (LastLoggedCrosshairSpread < 0.0f || FMath::Abs(SpreadFactor - LastLoggedCrosshairSpread) >= CrosshairLogThreshold)
	{
		LastLoggedCrosshairSpread = SpreadFactor;
		UE_LOG(LogMosesHUD, Log, TEXT("[HUD][CL] Crosshair Spread=%.2f"), SpreadFactor);
	}
}

// --------------------------------------------------------------------
// PlayerState Handlers
// --------------------------------------------------------------------

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

// --------------------------------------------------------------------
// MatchGameState Handlers
// --------------------------------------------------------------------

void UMosesMatchHUD::HandleMatchTimeChanged(int32 RemainingSeconds)
{
	if (MatchCountdownText)
	{
		MatchCountdownText->SetText(FText::FromString(ToMMSS(RemainingSeconds)));
	}
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

int32 UMosesMatchHUD::GetPhasePriority(EMosesMatchPhase Phase)
{
	switch (Phase)
	{
	case EMosesMatchPhase::WaitingForPlayers: return 0;
	case EMosesMatchPhase::Warmup:           return 1;
	case EMosesMatchPhase::Combat:           return 2;
	case EMosesMatchPhase::Result:           return 3;
	default:                                return -1;
	}
}

void UMosesMatchHUD::HandleMatchPhaseChanged(EMosesMatchPhase NewPhase)
{
	const int32 OldP = GetPhasePriority(LastAppliedPhase);
	const int32 NewP = GetPhasePriority(NewPhase);

	// [FIX] 역행 이벤트(Combat->Warmup 같은)는 무시
	if (NewP < OldP)
	{
		UE_LOG(LogMosesPhase, Warning,
			TEXT("[HUD][CL] PhaseChanged IGNORE (rollback) New=%s Old=%s"),
			*UEnum::GetValueAsString(NewPhase),
			*UEnum::GetValueAsString(LastAppliedPhase));
		return;
	}

	LastAppliedPhase = NewPhase;

	UE_LOG(LogMosesPhase, Warning, TEXT("[HUD][CL] PhaseChanged APPLY New=%s PhaseTextPtr=%s"),
		*UEnum::GetValueAsString(NewPhase),
		*GetNameSafe(PhaseText));

	if (!PhaseText)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] PhaseText is NULL. Check WBP TextBlock name == 'PhaseText'"));
		return;
	}

	PhaseText->SetText(GetPhaseText_KR(NewPhase));

	UE_LOG(LogMosesPhase, Warning, TEXT("[HUD][CL] PhaseText SET -> %s"),
		*GetPhaseText_KR(NewPhase).ToString());
}

void UMosesMatchHUD::HandleAnnouncementChanged(const FMosesAnnouncementState& State)
{
	if (AnnouncementWidget)
	{
		AnnouncementWidget->UpdateAnnouncement(State);
	}
}

// --------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------

FString UMosesMatchHUD::ToMMSS(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 MM = Clamped / 60;
	const int32 SS = Clamped % 60;
	return FString::Printf(TEXT("%02d:%02d"), MM, SS);
}
