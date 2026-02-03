#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchHUD.h"

#include "UE5_Multi_Shooter/MosesPlayerController.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"

#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"

#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchAnnouncementWidget.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesCrosshairWidget.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesScopeWidget.h"

#include "UE5_Multi_Shooter/Match/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Match/Flag/MosesFlagSpot.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesCaptureProgressWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// ============================================================================
// Local Helpers (HUD 안전 유틸)
// ============================================================================

static void MosesHUD_SetTextIfValid(UTextBlock* TB, const FText& Text)
{
	if (TB)
	{
		TB->SetText(Text);
	}
}

static void MosesHUD_SetVisibleIfValid(UWidget* W, bool bVisible)
{
	if (W)
	{
		W->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

static void MosesHUD_SetImageVisibleIfValid(UImage* Img, bool bVisible)
{
	if (Img)
	{
		Img->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

// ============================================================================
// Ctor
// ============================================================================

UMosesMatchHUD::UMosesMatchHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ============================================================================
// UUserWidget Lifecycle
// ============================================================================

void UMosesMatchHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Init MatchHUD=%s OwningPlayer=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwningPlayer()));

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] PhaseText Bind=%s (Check WBP name == PhaseText)"),
		*GetNameSafe(PhaseText));

	// [CAPTURE] 내부 위젯 캐시(한 번만). CaptureProgress는 WBP_CombatHUD에 배치돼 있어야 한다.
	CacheCaptureProgress_InternalWidgets();

	// 1) 즉시 바인딩 시도
	BindToPlayerState();
	BindToGameState_Match();

	// 2) 초기값 세팅
	RefreshInitial();

	// 3) 바인딩이 늦게 되는 케이스 재시도 (Tick 금지 → Timer)
	StartBindRetry();

	// 4) Crosshair Update Loop (표시 전용)
	StartCrosshairUpdate();

	// 5) ScopeWidget 초기 숨김
	SetScopeVisible_Local(false);

	// [CAPTURE] PS가 이미 바인딩 되어 있으면 즉시 캡처도 바인딩 시도
	BindToCaptureComponent_FromPlayerState();
}

void UMosesMatchHUD::NativeDestruct()
{
	// --------------------------------------------------------------------
	// [CAPTURE] 먼저 해제 (PS가 사라지기 전에 안전하게)
	// --------------------------------------------------------------------
	UnbindCaptureComponent();

	// --------------------------------------------------------------------
	// Combat 먼저 해제(PS 해제 전에)
	// --------------------------------------------------------------------
	StopBindRetry();
	StopCrosshairUpdate();

	UnbindCombatComponent();

	if (AMosesPlayerState* PS = CachedPlayerState.Get())
	{
		PS->OnHealthChanged.RemoveAll(this);
		PS->OnShieldChanged.RemoveAll(this);
		PS->OnScoreChanged.RemoveAll(this);
		PS->OnDeathsChanged.RemoveAll(this);

		PS->OnPlayerCapturesChanged.RemoveAll(this);
		PS->OnPlayerZombieKillsChanged.RemoveAll(this);

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
// Scope UI (Local cosmetic only)
// ============================================================================

void UMosesMatchHUD::SetScopeVisible_Local(bool bVisible)
{
	if (ScopeWidget)
	{
		ScopeWidget->SetScopeVisible(bVisible);
	}
}

void UMosesMatchHUD::TryBroadcastCaptureSuccess_Once()
{
	// 로컬 중복 방지(프레임/틱/다중 델리게이트 호출 방지)
	if (bCaptureSuccessAnnounced_Local)
	{
		return;
	}

	AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(GetOwningPlayer());
	if (!MosesPC)
	{
		return;
	}

	// 서버에 요청(서버가 MatchGameState -> Announcement로 RepNotify)
	bCaptureSuccessAnnounced_Local = true;
	MosesPC->Server_RequestCaptureSuccessAnnouncement();

	UE_LOG(LogMosesHUD, Warning, TEXT("[ANN][CL] Request CaptureSuccessAnnouncement -> PC=%s"), *GetNameSafe(MosesPC));
}


// ============================================================================
// Bind / Retry
// ============================================================================

bool UMosesMatchHUD::IsPlayerStateBound() const
{
	return CachedPlayerState.IsValid();
}

bool UMosesMatchHUD::IsMatchGameStateBound() const
{
	return CachedMatchGameState.IsValid();
}

bool UMosesMatchHUD::IsCombatComponentBound() const
{
	return CachedCombatComponent.IsValid();
}

bool UMosesMatchHUD::IsCaptureComponentBound() const
{
	return CachedCaptureComponent.IsValid();
}

void UMosesMatchHUD::StartBindRetry()
{
	// PS / GS / Combat / Capture가 모두 바인딩이면 재시도 필요 없음
	if (IsPlayerStateBound() && IsMatchGameStateBound() && IsCombatComponentBound() && IsCaptureComponentBound())
	{
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

	if (!IsPlayerStateBound())
	{
		BindToPlayerState();
	}

	if (!IsMatchGameStateBound())
	{
		BindToGameState_Match();
	}

	// PS 바인딩이 되면 Combat도 재시도
	if (!IsCombatComponentBound())
	{
		BindToCombatComponent_FromPlayerState();
	}

	// [CAPTURE] PS 바인딩이 되면 Capture도 재시도
	if (!IsCaptureComponentBound())
	{
		BindToCaptureComponent_FromPlayerState();
	}

	ApplySnapshotFromMatchGameState();

	const bool bDone = IsPlayerStateBound() && IsMatchGameStateBound() && IsCombatComponentBound() && IsCaptureComponentBound();
	if (bDone)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry DONE Try=%d"), BindRetryTryCount);
		StopBindRetry();
		return;
	}

	if (BindRetryTryCount >= BindRetryMaxTry)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry GIVEUP Try=%d (PS=%d GS=%d Combat=%d Capture=%d)"),
			BindRetryTryCount,
			IsPlayerStateBound() ? 1 : 0,
			IsMatchGameStateBound() ? 1 : 0,
			IsCombatComponentBound() ? 1 : 0,
			IsCaptureComponentBound() ? 1 : 0);

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

	if (CachedPlayerState.Get() == PS)
	{
		// PS는 동일하더라도 Combat/Capture가 바뀌는 경우가 있어 보강한다.
		BindToCombatComponent_FromPlayerState();
		BindToCaptureComponent_FromPlayerState();
		return;
	}

	// --------------------------------------------------------------------
	// 기존 PS 바인딩 제거
	// --------------------------------------------------------------------
	if (AMosesPlayerState* OldPS = CachedPlayerState.Get())
	{
		OldPS->OnHealthChanged.RemoveAll(this);
		OldPS->OnShieldChanged.RemoveAll(this);
		OldPS->OnScoreChanged.RemoveAll(this);
		OldPS->OnDeathsChanged.RemoveAll(this);

		OldPS->OnPlayerCapturesChanged.RemoveAll(this);
		OldPS->OnPlayerZombieKillsChanged.RemoveAll(this);

		OldPS->OnAmmoChanged.RemoveAll(this);
		OldPS->OnGrenadeChanged.RemoveAll(this);
	}

	CachedPlayerState = PS;

	// --------------------------------------------------------------------
	// 새 PS 바인딩
	// --------------------------------------------------------------------
	PS->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	PS->OnShieldChanged.AddUObject(this, &ThisClass::HandleShieldChanged);
	PS->OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	PS->OnDeathsChanged.AddUObject(this, &ThisClass::HandleDeathsChanged);

	PS->OnPlayerCapturesChanged.AddUObject(this, &ThisClass::HandleCapturesChanged);
	PS->OnPlayerZombieKillsChanged.AddUObject(this, &ThisClass::HandleZombieKillsChanged);

	// PS의 AmmoChanged는 fallback/초기값용으로 유지하되,
	// "즉시 전환"은 CombatComponent 경로가 우선이다.
	PS->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged_FromPS);
	PS->OnGrenadeChanged.AddUObject(this, &ThisClass::HandleGrenadeChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound PlayerState delegates PS=%s"), *GetNameSafe(PS));

	// --------------------------------------------------------------------
	// PS 바인딩 후 Combat / Capture 바인딩
	// --------------------------------------------------------------------
	BindToCombatComponent_FromPlayerState();
	BindToCaptureComponent_FromPlayerState();

	// --------------------------------------------------------------------
	// 바인딩 직후 스냅샷 강제 반영 (LateJoin / Retry 타이밍 안정화)
	// --------------------------------------------------------------------
	HandleDeathsChanged(PS->GetDeaths());
	HandleCapturesChanged(PS->GetCaptures());
	HandleZombieKillsChanged(PS->GetZombieKills());
}

void UMosesMatchHUD::BindToCombatComponent_FromPlayerState()
{
	AMosesPlayerState* PS = CachedPlayerState.Get();
	if (!PS)
	{
		return;
	}

	UMosesCombatComponent* Combat = PS->FindComponentByClass<UMosesCombatComponent>();
	if (!Combat)
	{
		return;
	}

	if (CachedCombatComponent.Get() == Combat)
	{
		return;
	}

	UnbindCombatComponent();

	CachedCombatComponent = Combat;

	Combat->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged_FromCombat);
	Combat->OnEquippedChanged.AddUObject(this, &ThisClass::HandleEquippedChanged_FromCombat);
	Combat->OnReloadingChanged.AddUObject(this, &ThisClass::HandleReloadingChanged_FromCombat);
	Combat->OnSlotsStateChanged.AddUObject(this, &ThisClass::HandleSlotsStateChanged_FromCombat);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound CombatComponent delegates Combat=%s PS=%s"),
		*GetNameSafe(Combat), *GetNameSafe(PS));

	// 바인딩 직후 스냅샷 반영
	HandleAmmoChanged_FromCombat(Combat->GetCurrentMagAmmo(), Combat->GetCurrentReserveAmmo());
}

void UMosesMatchHUD::UnbindCombatComponent()
{
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		Combat->OnAmmoChanged.RemoveAll(this);
		Combat->OnEquippedChanged.RemoveAll(this);
		Combat->OnReloadingChanged.RemoveAll(this);
		Combat->OnSlotsStateChanged.RemoveAll(this);

		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Unbound CombatComponent delegates Combat=%s"), *GetNameSafe(Combat));
	}

	CachedCombatComponent.Reset();
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

	if (CachedMatchGameState.Get() == GS)
	{
		return;
	}

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

	ApplySnapshotFromMatchGameState();
}

void UMosesMatchHUD::ApplySnapshotFromMatchGameState()
{
	AMosesMatchGameState* GS = CachedMatchGameState.Get();
	if (!GS)
	{
		return;
	}

	HandleMatchTimeChanged(GS->GetRemainingSeconds());
	HandleMatchPhaseChanged(GS->GetMatchPhase());
	HandleAnnouncementChanged(GS->GetAnnouncementState());

	// Combat이 바인딩되어 있으면 현재 탄약도 강제 적용
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		HandleAmmoChanged_FromCombat(Combat->GetCurrentMagAmmo(), Combat->GetCurrentReserveAmmo());
	}
}

void UMosesMatchHUD::RefreshInitial()
{
	HandleScoreChanged(0);
	HandleDeathsChanged(0);

	HandleCapturesChanged(0);
	HandleZombieKillsChanged(0);

	HandleAmmoChanged_FromPS(0, 0);
	HandleGrenadeChanged(0);

	HandleHealthChanged(100.f, 100.f);
	HandleShieldChanged(100.f, 100.f);

	ApplySnapshotFromMatchGameState();

	// [CAPTURE] 초기에는 숨김이 정답
	if (CaptureProgress)
	{
		CaptureProgress->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ============================================================================
// [DAY7] Crosshair Update (표시 전용)
// ============================================================================

void UMosesMatchHUD::StartCrosshairUpdate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CrosshairWidget)
	{
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
	return FMath::Clamp(Speed2D / 600.0f, 0.0f, 1.0f);
}

void UMosesMatchHUD::TickCrosshairUpdate()
{
	if (!CrosshairWidget)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(PC);

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	UMosesCombatComponent* Combat = PS->FindComponentByClass<UMosesCombatComponent>();
	if (!Combat)
	{
		return;
	}

	const FGameplayTag WeaponId = Combat->GetEquippedWeaponId();

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UMosesWeaponRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UMosesWeaponRegistrySubsystem>() : nullptr;

	const UMosesWeaponData* WeaponData = Registry ? Registry->ResolveWeaponData(WeaponId) : nullptr;

	const bool bIsSniper = (WeaponData && WeaponData->WeaponId == FMosesGameplayTags::Get().Weapon_Sniper_A);
	const bool bScopeOn = (MosesPC ? MosesPC->IsScopeActive_Local() : false);

	// 스나이퍼는 스코프 ON/OFF 모두 크로스헤어 숨김(정책)
	if (bIsSniper)
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	CrosshairWidget->SetVisibility(ESlateVisibility::Visible);

	const float SpreadFactor = CalculateCrosshairSpreadFactor_Local();
	CrosshairWidget->SetSpreadFactor(SpreadFactor);

	if (LastLoggedCrosshairSpread < 0.0f || FMath::Abs(SpreadFactor - LastLoggedCrosshairSpread) >= CrosshairLogThreshold)
	{
		LastLoggedCrosshairSpread = SpreadFactor;
		UE_LOG(LogMosesHUD, Log, TEXT("[HUD][CL] Crosshair Spread=%.2f Weapon=%s Scope=%d"),
			SpreadFactor,
			*WeaponId.ToString(),
			bScopeOn ? 1 : 0);
	}
}

// ============================================================================
// PlayerState Handlers (SSOT)
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

void UMosesMatchHUD::HandleCapturesChanged(int32 NewCaptures)
{
	if (CapturesAmount)
	{
		CapturesAmount->SetText(FText::AsNumber(NewCaptures));
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] PlayerCapturesChanged=%d"), NewCaptures);
}

void UMosesMatchHUD::HandleZombieKillsChanged(int32 NewZombieKills)
{
	if (ZombieKillsAmount)
	{
		ZombieKillsAmount->SetText(FText::AsNumber(NewZombieKills));
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] PlayerZombieKillsChanged=%d"), NewZombieKills);
}

void UMosesMatchHUD::HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve)
{
	// PS 경로는 fallback/초기값용. 실제 즉시 전환은 Combat 경로가 담당한다.
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
// CombatComponent Handlers
// ============================================================================

void UMosesMatchHUD::HandleAmmoChanged_FromCombat(int32 Mag, int32 Reserve)
{
	if (WeaponAmmoAmount)
	{
		WeaponAmmoAmount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, Reserve)));
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL][STEP3] AmmoChanged FromCombat %d/%d"), Mag, Reserve);
	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();
}

void UMosesMatchHUD::HandleEquippedChanged_FromCombat(int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/)
{
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		HandleAmmoChanged_FromCombat(Combat->GetCurrentMagAmmo(), Combat->GetCurrentReserveAmmo());
	}

	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();
}

void UMosesMatchHUD::HandleReloadingChanged_FromCombat(bool /*bReloading*/)
{
	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] ReloadingChanged -> UpdateHeader+Slots"));
}

void UMosesMatchHUD::HandleSlotsStateChanged_FromCombat(int32 ChangedSlotOr0ForAll)
{
	(void)ChangedSlotOr0ForAll;

	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] SlotsStateChanged Slot=%d -> UpdateHeader+Slots"), ChangedSlotOr0ForAll);
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

// ============================================================================
// Helpers
// ============================================================================

FString UMosesMatchHUD::ToMMSS(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 MM = Clamped / 60;
	const int32 SS = Clamped % 60;
	return FString::Printf(TEXT("%02d:%02d"), MM, SS);
}

void UMosesMatchHUD::UpdateCurrentWeaponHeader()
{
	UMosesCombatComponent* Combat = CachedCombatComponent.Get();
	if (!Combat)
	{
		MosesHUD_SetTextIfValid(Text_CurrentWeaponName, FText::FromString(TEXT("-")));
		MosesHUD_SetTextIfValid(Text_CurrentSlot, FText::FromString(TEXT("Slot: -")));
		MosesHUD_SetVisibleIfValid(Text_Reloading, false);
		return;
	}

	const int32 CurSlot = Combat->GetCurrentSlot();
	const FGameplayTag CurWeaponId = Combat->GetEquippedWeaponId();

	const FString WeaponNameStr = CurWeaponId.IsValid() ? CurWeaponId.ToString() : TEXT("-");
	MosesHUD_SetTextIfValid(Text_CurrentWeaponName, FText::FromString(WeaponNameStr));
	MosesHUD_SetTextIfValid(Text_CurrentSlot, FText::FromString(FString::Printf(TEXT("Slot: %d"), CurSlot)));

	MosesHUD_SetVisibleIfValid(Text_Reloading, Combat->IsReloading());
}

void UMosesMatchHUD::UpdateSlotPanels_All()
{
	UMosesCombatComponent* Combat = CachedCombatComponent.Get();
	if (!Combat)
	{
		return;
	}

	const int32 EquippedSlot = Combat->GetCurrentSlot();
	const bool bReloading = Combat->IsReloading();

	auto MakeAmmoTextLocal = [](int32 Mag, int32 Reserve) -> FText
		{
			return FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, Reserve));
		};

	auto ApplySlotUI = [&](int32 SlotIndex, UTextBlock* WeaponNameTB, UTextBlock* AmmoTB, UImage* EquippedImg, UTextBlock* ReloadingTB)
		{
			const FGameplayTag WeaponId = Combat->GetWeaponIdForSlot(SlotIndex);
			const bool bHasWeapon = WeaponId.IsValid();
			const bool bEquipped = (SlotIndex == EquippedSlot);

			if (bHasWeapon)
			{
				MosesHUD_SetTextIfValid(WeaponNameTB, FText::FromString(WeaponId.ToString()));
			}
			else
			{
				MosesHUD_SetTextIfValid(WeaponNameTB, FText::FromString(TEXT("-")));
			}

			if (bHasWeapon)
			{
				const int32 Mag = Combat->GetMagAmmoForSlot(SlotIndex);
				const int32 Res = Combat->GetReserveAmmoForSlot(SlotIndex);
				MosesHUD_SetTextIfValid(AmmoTB, MakeAmmoTextLocal(Mag, Res));
			}
			else
			{
				MosesHUD_SetTextIfValid(AmmoTB, MakeAmmoTextLocal(0, 0));
			}

			MosesHUD_SetImageVisibleIfValid(EquippedImg, bEquipped);

			if (ReloadingTB)
			{
				ReloadingTB->SetText(FText::FromString(TEXT("RELOAD")));
				ReloadingTB->SetVisibility((bEquipped && bReloading) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		};

	ApplySlotUI(1, Text_Slot1_WeaponName, Text_Slot1_Ammo, Img_Slot1_Equipped, Text_Slot1_Reloading);
	ApplySlotUI(2, Text_Slot2_WeaponName, Text_Slot2_Ammo, Img_Slot2_Equipped, Text_Slot2_Reloading);
	ApplySlotUI(3, Text_Slot3_WeaponName, Text_Slot3_Ammo, Img_Slot3_Equipped, Text_Slot3_Reloading);
	ApplySlotUI(4, Text_Slot4_WeaponName, Text_Slot4_Ammo, Img_Slot4_Equipped, Text_Slot4_Reloading);
}

// ============================================================================
// [CAPTURE] Cache internal widgets inside CaptureProgress
// ============================================================================

void UMosesMatchHUD::CacheCaptureProgress_InternalWidgets()
{
	if (bCaptureInternalCached)
	{
		return;
	}
	bCaptureInternalCached = true;

	if (!CaptureProgress)
	{
		UE_LOG(LogMosesHUD, Verbose, TEXT("[CAPTURE][HUD][CL] CaptureProgress NULL. Check WBP_CombatHUD widget name == 'CaptureProgress'"));
		return;
	}

	// WBP_CaptureProgress 내부 위젯 이름(필수)
	Capture_ProgressBar = Cast<UProgressBar>(CaptureProgress->GetWidgetFromName(TEXT("Progress_Capture")));
	Capture_TextPercent = Cast<UTextBlock>(CaptureProgress->GetWidgetFromName(TEXT("Text_Percent")));
	Capture_TextWarning = Cast<UTextBlock>(CaptureProgress->GetWidgetFromName(TEXT("Text_Warning")));

	UE_LOG(LogMosesHUD, Warning, TEXT("[CAPTURE][HUD][CL] CacheInternal OK PB=%s Percent=%s Warning=%s"),
		*GetNameSafe(Capture_ProgressBar),
		*GetNameSafe(Capture_TextPercent),
		*GetNameSafe(Capture_TextWarning));

	// 기본은 숨김
	CaptureProgress->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================================
// [CAPTURE] Bind / Unbind
// ============================================================================

void UMosesMatchHUD::BindToCaptureComponent_FromPlayerState()
{
	AMosesPlayerState* PS = CachedPlayerState.Get();
	if (!PS)
	{
		return;
	}

	UMosesCaptureComponent* CaptureComp = PS->FindComponentByClass<UMosesCaptureComponent>();
	if (!CaptureComp)
	{
		return;
	}

	if (CachedCaptureComponent.Get() == CaptureComp)
	{
		return;
	}

	UnbindCaptureComponent();

	CachedCaptureComponent = CaptureComp;

	// ✅ 시그니처가 정확히 일치하는 핸들러로 바인딩
	CaptureComp->OnCaptureStateChanged.AddUObject(this, &ThisClass::HandleCaptureStateChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[CAPTURE][HUD][CL] Bound CaptureComponent=%s PS=%s"),
		*GetNameSafe(CaptureComp),
		*GetNameSafe(PS));
}

void UMosesMatchHUD::UnbindCaptureComponent()
{
	if (UMosesCaptureComponent* CaptureComp = CachedCaptureComponent.Get())
	{
		CaptureComp->OnCaptureStateChanged.RemoveAll(this);
		UE_LOG(LogMosesHUD, Warning, TEXT("[CAPTURE][HUD][CL] Unbound CaptureComponent=%s"), *GetNameSafe(CaptureComp));
	}

	CachedCaptureComponent.Reset();
}

// ============================================================================
// [CAPTURE] Delegate Handler
// ============================================================================

void UMosesMatchHUD::HandleCaptureStateChanged(bool bActive, float Progress01, float WarningFloat, TWeakObjectPtr<AMosesFlagSpot> Spot)
{
	CacheCaptureProgress_InternalWidgets();

	if (!CaptureProgress)
	{
		return;
	}

	if (!bActive)
	{
		// 캡처 종료/취소 시 숨김 + 성공 요청 플래그 리셋
		CaptureProgress->SetVisibility(ESlateVisibility::Collapsed);

		// 다음 캡처를 위해 리셋
		bCaptureSuccessAnnounced_Local = false;
		return;
	}

	// 캡처 진행 중: 표시 + 값 갱신
	CaptureProgress->SetVisibility(ESlateVisibility::Visible);

	const float Clamped = FMath::Clamp(Progress01, 0.0f, 1.0f);

	if (Capture_ProgressBar)
	{
		Capture_ProgressBar->SetPercent(Clamped);
	}

	if (Capture_TextPercent)
	{
		const int32 PercentInt = FMath::RoundToInt(Clamped * 100.0f);
		Capture_TextPercent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), PercentInt)));
	}

	// WarningFloat 의미가 무엇이든 > 0이면 경고 ON
	const bool bWarningOn = (WarningFloat > 0.0f);

	if (Capture_TextWarning)
	{
		Capture_TextWarning->SetVisibility(bWarningOn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// ✅ [CAPTURE SUCCESS] 진행률이 1.0이 되면 “캡쳐 성공” 브로드캐스트 요청(딱 1번)
	// 서버가 실제 캡처 성공을 확정하고 MatchGameState로 Announcement를 RepNotify 한다.
	if (Clamped >= 1.0f - KINDA_SMALL_NUMBER)
	{
		TryBroadcastCaptureSuccess_Once();
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[CAPTURE][HUD][CL] StateChanged Active=1 Pct=%.2f WarnF=%.2f Spot=%s"),
		Clamped,
		WarningFloat,
		*GetNameSafe(Spot.Get()));
}
