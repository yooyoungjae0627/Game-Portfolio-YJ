#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchHUD.h"

#include "UE5_Multi_Shooter/MosesPlayerController.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"

#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Match/UI/Match/MosesMatchAnnouncementWidget.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesCrosshairWidget.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesScopeWidget.h"

#include "UE5_Multi_Shooter/Match/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Match/Flag/MosesFlagSpot.h"
#include "UE5_Multi_Shooter/Match/UI/Match/MosesCaptureProgressWidget.h"

#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"

// ============================================================================
// Local Helpers
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
// Aim UI
// ============================================================================
bool UMosesMatchHUD::IsSniperEquipped_Local() const
{
	const UMosesCombatComponent* Combat = CachedCombatComponent.Get();
	if (!Combat)
	{
		return false;
	}

	const FGameplayTag WeaponId = Combat->GetEquippedWeaponId();
	return (WeaponId == FMosesGameplayTags::Get().Weapon_Sniper_A);
}

void UMosesMatchHUD::UpdateAimWidgets_Immediate()
{
	AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(GetOwningPlayer());
	const bool bIsSniper = IsSniperEquipped_Local();
	const bool bScopeOn = (MosesPC ? MosesPC->IsScopeActive_Local() : false);

	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(bIsSniper ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (ScopeWidget)
	{
		const bool bShowScope = (bIsSniper && bScopeOn);
		ScopeWidget->SetScopeVisible(bShowScope);
	}
}

// ============================================================================
// UUserWidget Lifecycle
// ============================================================================
void UMosesMatchHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Init MatchHUD=%s OwningPlayer=%s"),
		*GetNameSafe(this), *GetNameSafe(GetOwningPlayer()));

	// 바인딩 완료 전까지 숨김
	SetVisibility(ESlateVisibility::Collapsed);

	CacheCaptureProgress_InternalWidgets();

	BindToPlayerState();
	BindToGameState_Match();
	BindToCombatComponent_FromPlayerState();
	BindToCaptureComponent_FromPlayerState();

	RefreshInitial();

	StartBindRetry();
	StartCrosshairUpdate();

	SetScopeVisible_Local(false);
	UpdateAimWidgets_Immediate();
}

void UMosesMatchHUD::NativeDestruct()
{
	StopRespawnNotice_Local();
	StopLocalPickupToast_Internal();

	UnbindCaptureComponent();
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
		PS->OnPlayerPvPKillsChanged.RemoveAll(this);
		PS->OnAmmoChanged.RemoveAll(this);
		PS->OnGrenadeChanged.RemoveAll(this);
		PS->OnDeathStateChanged.RemoveAll(this);
	}

	if (AMosesMatchGameState* GS = CachedMatchGameState.Get())
	{
		GS->OnMatchTimeChanged.RemoveAll(this);
		GS->OnMatchPhaseChanged.RemoveAll(this);
		GS->OnAnnouncementChanged.RemoveAll(this);
	}

	CachedPlayerState.Reset();
	CachedMatchGameState.Reset();

	Super::NativeDestruct();
}

// ============================================================================
// Public API
// ============================================================================
void UMosesMatchHUD::SetScopeVisible_Local(bool bVisible)
{
	if (ScopeWidget)
	{
		ScopeWidget->SetScopeVisible(bVisible);
	}
}

void UMosesMatchHUD::ShowPickupToast_Local(const FText& Text, float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !AnnouncementWidget)
	{
		return;
	}

	bLocalPickupToastActive = true;

	FMosesAnnouncementState S;
	S.bActive = true;
	S.Text = Text;
	AnnouncementWidget->UpdateAnnouncement(S);

	World->GetTimerManager().ClearTimer(LocalPickupToastTimerHandle);
	World->GetTimerManager().SetTimer(
		LocalPickupToastTimerHandle,
		this,
		&ThisClass::StopLocalPickupToast_Internal,
		FMath::Clamp(DurationSeconds, 0.2f, 10.0f),
		false);

	UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][CL] Toast SHOW Text='%s' Dur=%.2f"),
		*Text.ToString(), DurationSeconds);
}

void UMosesMatchHUD::StopLocalPickupToast_Internal()
{
	if (AnnouncementWidget)
	{
		FMosesAnnouncementState Off;
		Off.bActive = false;
		AnnouncementWidget->UpdateAnnouncement(Off);
	}

	bLocalPickupToastActive = false;
}

// ============================================================================
// Bind / Retry
// ============================================================================
bool UMosesMatchHUD::IsPlayerStateBound() const { return CachedPlayerState.IsValid(); }
bool UMosesMatchHUD::IsMatchGameStateBound() const { return CachedMatchGameState.IsValid(); }
bool UMosesMatchHUD::IsCombatComponentBound() const { return CachedCombatComponent.IsValid(); }
bool UMosesMatchHUD::IsCaptureComponentBound() const { return CachedCaptureComponent.IsValid(); }

void UMosesMatchHUD::StartBindRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(BindRetryHandle))
	{
		return;
	}

	SetVisibility(ESlateVisibility::Collapsed);

	BindRetryTryCount = 0;

	World->GetTimerManager().SetTimer(
		BindRetryHandle,
		this,
		&ThisClass::TryBindRetry,
		BindRetryInterval,
		true);
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
	}
}

void UMosesMatchHUD::TryBindRetry()
{
	BindRetryTryCount++;

	if (!IsPlayerStateBound()) { BindToPlayerState(); }
	if (!IsMatchGameStateBound()) { BindToGameState_Match(); }
	if (!IsCombatComponentBound()) { BindToCombatComponent_FromPlayerState(); }
	if (!IsCaptureComponentBound()) { BindToCaptureComponent_FromPlayerState(); }

	ApplySnapshotFromMatchGameState();
	UpdateAimWidgets_Immediate();

	const bool bEssentialReady = IsPlayerStateBound() && IsMatchGameStateBound() && IsCombatComponentBound();
	if (bEssentialReady)
	{
		SetVisibility(ESlateVisibility::Visible);
		StopBindRetry();
		return;
	}

	if (BindRetryTryCount >= BindRetryMaxTry)
	{
		SetVisibility(ESlateVisibility::Visible);
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
		BindToCombatComponent_FromPlayerState();
		BindToCaptureComponent_FromPlayerState();

		HandleHealthChanged(PS->GetHealth_Current(), PS->GetHealth_Max());
		HandleShieldChanged(PS->GetShield_Current(), PS->GetShield_Max());
		HandleScoreChanged(FMath::RoundToInt(PS->GetScore()));
		HandlePvPKillsChanged(PS->GetPvPKills());
		HandleCapturesChanged(PS->GetCaptures());
		HandleZombieKillsChanged(PS->GetZombieKills());
		HandleDeathStateChanged(PS->IsDead(), PS->GetRespawnEndServerTime());
		return;
	}

	if (AMosesPlayerState* OldPS = CachedPlayerState.Get())
	{
		OldPS->OnHealthChanged.RemoveAll(this);
		OldPS->OnShieldChanged.RemoveAll(this);
		OldPS->OnScoreChanged.RemoveAll(this);
		OldPS->OnDeathsChanged.RemoveAll(this);
		OldPS->OnPlayerCapturesChanged.RemoveAll(this);
		OldPS->OnPlayerZombieKillsChanged.RemoveAll(this);
		OldPS->OnPlayerPvPKillsChanged.RemoveAll(this);
		OldPS->OnAmmoChanged.RemoveAll(this);
		OldPS->OnGrenadeChanged.RemoveAll(this);
		OldPS->OnDeathStateChanged.RemoveAll(this);
	}

	CachedPlayerState = PS;

	PS->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	PS->OnShieldChanged.AddUObject(this, &ThisClass::HandleShieldChanged);
	PS->OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	PS->OnPlayerCapturesChanged.AddUObject(this, &ThisClass::HandleCapturesChanged);
	PS->OnPlayerZombieKillsChanged.AddUObject(this, &ThisClass::HandleZombieKillsChanged);
	PS->OnPlayerPvPKillsChanged.AddUObject(this, &ThisClass::HandlePvPKillsChanged);
	PS->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged_FromPS);
	PS->OnGrenadeChanged.AddUObject(this, &ThisClass::HandleGrenadeChanged);

	PS->OnDeathStateChanged.AddUObject(this, &ThisClass::HandleDeathStateChanged);

	HandleHealthChanged(PS->GetHealth_Current(), PS->GetHealth_Max());
	HandleShieldChanged(PS->GetShield_Current(), PS->GetShield_Max());
	HandleScoreChanged(FMath::RoundToInt(PS->GetScore()));
	HandleCapturesChanged(PS->GetCaptures());
	HandleZombieKillsChanged(PS->GetZombieKills());
	HandlePvPKillsChanged(PS->GetPvPKills());
	HandleDeathStateChanged(PS->IsDead(), PS->GetRespawnEndServerTime());

	BindToCombatComponent_FromPlayerState();
	BindToCaptureComponent_FromPlayerState();

	UpdateAimWidgets_Immediate();
}

void UMosesMatchHUD::BindToCombatComponent_FromPlayerState()
{
	AMosesPlayerState* PS = CachedPlayerState.Get();
	if (!PS)
	{
		return;
	}

	UMosesCombatComponent* Combat = PS->GetCombatComponent(); // SSOT
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

	Combat->OnAmmoChangedEx.AddUObject(this, &ThisClass::HandleAmmoChangedEx_FromCombat);
	Combat->OnEquippedChanged.AddUObject(this, &ThisClass::HandleEquippedChanged_FromCombat);
	Combat->OnReloadingChanged.AddUObject(this, &ThisClass::HandleReloadingChanged_FromCombat);
	Combat->OnSlotsStateChanged.AddUObject(this, &ThisClass::HandleSlotsStateChanged_FromCombat);

	HandleAmmoChangedEx_FromCombat(
		Combat->GetCurrentMagAmmo(),
		Combat->GetCurrentReserveAmmo(),
		Combat->GetCurrentReserveMax());

	UpdateAimWidgets_Immediate();
}

void UMosesMatchHUD::UnbindCombatComponent()
{
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		Combat->OnAmmoChanged.RemoveAll(this);
		Combat->OnAmmoChangedEx.RemoveAll(this);
		Combat->OnEquippedChanged.RemoveAll(this);
		Combat->OnReloadingChanged.RemoveAll(this);
		Combat->OnSlotsStateChanged.RemoveAll(this);
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

	// 로컬 리스폰/픽업 토스트 중엔 전원 공지 덮지 않음
	if (!bLocalRespawnNoticeActive && !bLocalPickupToastActive)
	{
		HandleAnnouncementChanged(GS->GetAnnouncementState());
	}

	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		HandleAmmoChangedEx_FromCombat(
			Combat->GetCurrentMagAmmo(),
			Combat->GetCurrentReserveAmmo(),
			Combat->GetCurrentReserveMax());
	}

	UpdateAimWidgets_Immediate();
}

void UMosesMatchHUD::RefreshInitial()
{
	AMosesPlayerState* PS = CachedPlayerState.Get();
	if (PS)
	{
		HandleScoreChanged(FMath::RoundToInt(PS->GetScore()));
		HandlePvPKillsChanged(PS->GetPvPKills());
		HandleCapturesChanged(PS->GetCaptures());
		HandleZombieKillsChanged(PS->GetZombieKills());

		HandleHealthChanged(PS->GetHealth_Current(), PS->GetHealth_Max());
		HandleShieldChanged(PS->GetShield_Current(), PS->GetShield_Max());

		HandleDeathStateChanged(PS->IsDead(), PS->GetRespawnEndServerTime());
	}
	else
	{
		HandleScoreChanged(0);
		HandlePvPKillsChanged(0);
		HandleCapturesChanged(0);
		HandleZombieKillsChanged(0);
		HandleHealthChanged(0.f, 0.f);
		HandleShieldChanged(0.f, 0.f);
	}

	ApplySnapshotFromMatchGameState();

	if (CaptureProgress)
	{
		CaptureProgress->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateAimWidgets_Immediate();
}

// ============================================================================
// Crosshair Update
// ============================================================================
void UMosesMatchHUD::StartCrosshairUpdate()
{
	UWorld* World = GetWorld();
	if (!World || !CrosshairWidget)
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
	UpdateAimWidgets_Immediate();

	if (!CrosshairWidget)
	{
		return;
	}

	if (IsSniperEquipped_Local())
	{
		return;
	}

	const float SpreadFactor = CalculateCrosshairSpreadFactor_Local();
	CrosshairWidget->SetSpreadFactor(SpreadFactor);

	if (LastLoggedCrosshairSpread < 0.0f || FMath::Abs(SpreadFactor - LastLoggedCrosshairSpread) >= CrosshairLogThreshold)
	{
		LastLoggedCrosshairSpread = SpreadFactor;

		UMosesCombatComponent* Combat = CachedCombatComponent.Get();
		const FGameplayTag WeaponId = Combat ? Combat->GetEquippedWeaponId() : FGameplayTag();

		AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(GetOwningPlayer());
		const bool bScopeOn = (MosesPC ? MosesPC->IsScopeActive_Local() : false);

		UE_LOG(LogMosesHUD, Log, TEXT("[HUD][CL] Crosshair Spread=%.2f Weapon=%s Scope=%d"),
			SpreadFactor, *WeaponId.ToString(), bScopeOn ? 1 : 0);
	}
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

void UMosesMatchHUD::HandleCapturesChanged(int32 NewCaptures)
{
	if (CapturesAmount)
	{
		CapturesAmount->SetText(FText::AsNumber(NewCaptures));
	}
}

void UMosesMatchHUD::HandleZombieKillsChanged(int32 NewZombieKills)
{
	if (ZombieKillsAmount)
	{
		ZombieKillsAmount->SetText(FText::AsNumber(NewZombieKills));
	}
}

void UMosesMatchHUD::HandlePvPKillsChanged(int32 NewPvPKills)
{
	if (DefeatsAmount)
	{
		DefeatsAmount->SetText(FText::AsNumber(NewPvPKills));
	}
}

void UMosesMatchHUD::HandleAmmoChanged_FromPS(int32 Mag, int32 Reserve)
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

void UMosesMatchHUD::HandleAmmoChangedEx_FromCombat(int32 Mag, int32 ReserveCur, int32 ReserveMax)
{
	(void)ReserveMax;

	if (WeaponAmmoAmount)
	{
		WeaponAmmoAmount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, ReserveCur)));
	}

	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();
}

// ============================================================================
// Respawn Notice (Victim only)
// ============================================================================
void UMosesMatchHUD::HandleDeathStateChanged(bool bIsDead, float InRespawnEndServerTime)
{
	if (!bIsDead)
	{
		StopRespawnNotice_Local();
		return;
	}

	StartRespawnNotice_Local(InRespawnEndServerTime);
}

void UMosesMatchHUD::StartRespawnNotice_Local(float InRespawnEndServerTime)
{
	UWorld* World = GetWorld();
	if (!World || !AnnouncementWidget)
	{
		return;
	}

	RespawnNoticeEndServerTime = InRespawnEndServerTime;
	bLocalRespawnNoticeActive = true;

	TickRespawnNotice_Local();

	World->GetTimerManager().ClearTimer(RespawnNoticeTimerHandle);
	World->GetTimerManager().SetTimer(
		RespawnNoticeTimerHandle,
		this,
		&ThisClass::TickRespawnNotice_Local,
		1.0f,
		true);
}

void UMosesMatchHUD::StopRespawnNotice_Local()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(RespawnNoticeTimerHandle);
	}

	RespawnNoticeEndServerTime = 0.f;
	bLocalRespawnNoticeActive = false;

	if (AnnouncementWidget)
	{
		FMosesAnnouncementState Off;
		Off.bActive = false;
		AnnouncementWidget->UpdateAnnouncement(Off);
	}
}

void UMosesMatchHUD::TickRespawnNotice_Local()
{
	if (!bLocalRespawnNoticeActive || !AnnouncementWidget)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGameStateBase* GSBase = World ? World->GetGameState() : nullptr;

	const float ServerNow = GSBase ? GSBase->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	const float Remaining = RespawnNoticeEndServerTime - ServerNow;
	const int32 RemainingSec = FMath::Max(0, FMath::CeilToInt(Remaining));

	FMosesAnnouncementState S;
	S.bActive = true;
	S.Text = FText::FromString(FString::Printf(TEXT("잠시후 리스폰됩니다. %d초"), RemainingSec));
	AnnouncementWidget->UpdateAnnouncement(S);

	if (RemainingSec <= 0)
	{
		StopRespawnNotice_Local();
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
	UpdateAimWidgets_Immediate();
}

void UMosesMatchHUD::HandleReloadingChanged_FromCombat(bool /*bReloading*/)
{
	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();
}

void UMosesMatchHUD::HandleSlotsStateChanged_FromCombat(int32 /*ChangedSlotOr0ForAll*/)
{
	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();
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
	default: return FText::FromString(TEXT("알 수 없음"));
	}
}

int32 UMosesMatchHUD::GetPhasePriority(EMosesMatchPhase Phase)
{
	switch (Phase)
	{
	case EMosesMatchPhase::WaitingForPlayers: return 0;
	case EMosesMatchPhase::Warmup: return 1;
	case EMosesMatchPhase::Combat: return 2;
	case EMosesMatchPhase::Result: return 3;
	default: return -1;
	}
}

void UMosesMatchHUD::HandleMatchPhaseChanged(EMosesMatchPhase NewPhase)
{
	const int32 OldP = GetPhasePriority(LastAppliedPhase);
	const int32 NewP = GetPhasePriority(NewPhase);

	if (NewP < OldP)
	{
		return;
	}

	LastAppliedPhase = NewPhase;

	if (!PhaseText)
	{
		return;
	}

	PhaseText->SetText(GetPhaseText_KR(NewPhase));
}

void UMosesMatchHUD::HandleAnnouncementChanged(const FMosesAnnouncementState& State)
{
	// 로컬 리스폰 공지 우선
	if (bLocalRespawnNoticeActive)
	{
		return;
	}

	// 픽업 토스트 표시 중이면 전원 공지 차단
	if (bLocalPickupToastActive)
	{
		return;
	}

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
		MosesHUD_SetTextIfValid(Text_CurrentSlot, FText::FromString(TEXT("슬롯: -")));
		MosesHUD_SetVisibleIfValid(Text_Reloading, false);
		return;
	}

	const int32 CurSlot = Combat->GetCurrentSlot();
	const FGameplayTag CurWeaponId = Combat->GetEquippedWeaponId();

	FText WeaponDisplayName = FText::FromString(TEXT("-"));

	switch (CurSlot)
	{
	case 1: WeaponDisplayName = FText::FromString(TEXT("라이플")); break;
	case 2: WeaponDisplayName = FText::FromString(TEXT("샷건")); break;
	case 3: WeaponDisplayName = FText::FromString(TEXT("스나이퍼")); break;
	case 4: WeaponDisplayName = FText::FromString(TEXT("수류탄 런처")); break;
	default: break;
	}

	if (WeaponDisplayName.EqualTo(FText::FromString(TEXT("-"))) && CurWeaponId.IsValid())
	{
		const FString TagStr = CurWeaponId.ToString();
		if (TagStr.Contains(TEXT("Rifle"), ESearchCase::IgnoreCase)) { WeaponDisplayName = FText::FromString(TEXT("라이플")); }
		else if (TagStr.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase)) { WeaponDisplayName = FText::FromString(TEXT("샷건")); }
		else if (TagStr.Contains(TEXT("Sniper"), ESearchCase::IgnoreCase)) { WeaponDisplayName = FText::FromString(TEXT("스나이퍼")); }
		else if (TagStr.Contains(TEXT("Grenade"), ESearchCase::IgnoreCase)) { WeaponDisplayName = FText::FromString(TEXT("수류탄 런처")); }
	}

	MosesHUD_SetTextIfValid(Text_CurrentWeaponName, WeaponDisplayName);
	MosesHUD_SetTextIfValid(Text_CurrentSlot, FText::FromString(FString::Printf(TEXT("슬롯: %d"), CurSlot)));
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

	auto ApplySlotUI = [&](int32 SlotIndex, UTextBlock* WeaponNameTB, UTextBlock* AmmoTB, UImage* EquippedImg, UTextBlock* ReloadingTB)
		{
			const FGameplayTag WeaponId = Combat->GetWeaponIdForSlot(SlotIndex);
			const bool bHasWeapon = WeaponId.IsValid();
			const bool bEquipped = (SlotIndex == EquippedSlot);

			MosesHUD_SetTextIfValid(WeaponNameTB, bHasWeapon ? FText::FromString(WeaponId.ToString()) : FText::FromString(TEXT("-")));

			if (bHasWeapon)
			{
				const int32 Mag = Combat->GetMagAmmoForSlot(SlotIndex);
				const int32 Res = Combat->GetReserveAmmoForSlot(SlotIndex);
				const int32 Max = Combat->GetReserveMaxForSlot(SlotIndex);

				MosesHUD_SetTextIfValid(AmmoTB, FText::FromString(FString::Printf(TEXT("%d | %d/%d"), Mag, Res, Max)));
			}
			else
			{
				MosesHUD_SetTextIfValid(AmmoTB, FText::FromString(TEXT("0 / 0")));
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
// [CAPTURE] Cache internal widgets
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
		return;
	}

	Capture_ProgressBar = Cast<UProgressBar>(CaptureProgress->GetWidgetFromName(TEXT("Progress_Capture")));
	Capture_TextPercent = Cast<UTextBlock>(CaptureProgress->GetWidgetFromName(TEXT("Text_Percent")));
	Capture_TextWarning = Cast<UTextBlock>(CaptureProgress->GetWidgetFromName(TEXT("Text_Warning")));

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
	CaptureComp->OnCaptureStateChanged.AddUObject(this, &ThisClass::HandleCaptureStateChanged);
}

void UMosesMatchHUD::UnbindCaptureComponent()
{
	if (UMosesCaptureComponent* CaptureComp = CachedCaptureComponent.Get())
	{
		CaptureComp->OnCaptureStateChanged.RemoveAll(this);
	}

	CachedCaptureComponent.Reset();
}

// ============================================================================
// [CAPTURE] Delegate Handler
// ============================================================================
void UMosesMatchHUD::HandleCaptureStateChanged(bool bActive, float Progress01, float WarningFloat, TWeakObjectPtr<AMosesFlagSpot> Spot)
{
	(void)Spot;

	CacheCaptureProgress_InternalWidgets();

	if (!CaptureProgress)
	{
		return;
	}

	if (!bActive)
	{
		CaptureProgress->SetVisibility(ESlateVisibility::Collapsed);
		bCaptureSuccessAnnounced_Local = false;
		return;
	}

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

	const bool bWarningOn = (WarningFloat > 0.0f);
	if (Capture_TextWarning)
	{
		Capture_TextWarning->SetVisibility(bWarningOn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Clamped >= 1.0f - KINDA_SMALL_NUMBER)
	{
		TryBroadcastCaptureSuccess_Once();
	}
}

void UMosesMatchHUD::TryBroadcastCaptureSuccess_Once()
{
	if (bCaptureSuccessAnnounced_Local)
	{
		return;
	}

	bCaptureSuccessAnnounced_Local = true;

	// 옵션: 여기서 PC RPC를 호출해 서버 공지 띄우고 싶으면 연결
	// AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOwningPlayer());
	// if (PC) { PC->Server_RequestCaptureSuccessAnnouncement(); }
}

// =========================================================
// [MOD] Headshot Toast (Owner Only)
// =========================================================
void UMosesMatchHUD::ShowHeadshotToast_Local(const FText& Text, float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !AnnouncementWidget)
	{
		return;
	}

	bLocalHeadshotToastActive = true;

	FMosesAnnouncementState S;
	S.bActive = true;
	S.Text = Text;
	AnnouncementWidget->UpdateAnnouncement(S);

	World->GetTimerManager().ClearTimer(LocalHeadshotToastTimerHandle);
	World->GetTimerManager().SetTimer(
		LocalHeadshotToastTimerHandle,
		this,
		&ThisClass::StopLocalHeadshotToast_Internal,
		FMath::Clamp(DurationSeconds, 0.2f, 10.0f),
		false);

	UE_LOG(LogMosesKillFeed, Warning, TEXT("[HEADSHOT][CL] Toast SHOW Text='%s' Dur=%.2f"),
		*Text.ToString(), DurationSeconds);
}

void UMosesMatchHUD::StopLocalHeadshotToast_Internal()
{
	if (AnnouncementWidget)
	{
		FMosesAnnouncementState Off;
		Off.bActive = false;
		AnnouncementWidget->UpdateAnnouncement(Off);
	}

	bLocalHeadshotToastActive = false;
}

static AMosesMatchGameState* MosesHUD_GetMatchGS(UWorld* World)
{
	return World ? World->GetGameState<AMosesMatchGameState>() : nullptr;
}

void UMosesMatchHUD::HandleResultStateChanged_Local(const FMosesMatchResultState& State)
{
	// Result 팝업은 1회만
	if (bResultPopupShown)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AMosesMatchGameState* GS = MosesHUD_GetMatchGS(World);
	if (!GS)
	{
		return;
	}

	// Result가 아니라면 무시
	if (!State.bIsResult && !GS->IsResultPhase())
	{
		return;
	}

	ShowResultPopup_Local(State);
}

void UMosesMatchHUD::ShowResultPopup_Local(const FMosesMatchResultState& State)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	AMosesPlayerState* MyPS = PC->GetPlayerState<AMosesPlayerState>();
	if (!MyPS)
	{
		return;
	}

	AMosesMatchGameState* GS = MosesHUD_GetMatchGS(World);
	if (!GS)
	{
		return;
	}

	// Opponent = PlayerArray에서 나 제외 첫 번째
	AMosesPlayerState* OppPS = nullptr;
	for (APlayerState* Raw : GS->PlayerArray)
	{
		AMosesPlayerState* PS = Cast<AMosesPlayerState>(Raw);
		if (!PS || PS == MyPS)
		{
			continue;
		}
		OppPS = PS;
		break;
	}

	if (!ResultPopupClass)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][CL] ResultPopupClass is null. Set it in HUD defaults."));
		return;
	}

	if (!ResultPopupInstance)
	{
		ResultPopupInstance = CreateWidget<UMosesResultPopupWidget>(PC, ResultPopupClass);
		if (!ResultPopupInstance)
		{
			return;
		}

		if (Overlay_CenterLayer)
		{
			Overlay_CenterLayer->AddChild(ResultPopupInstance);
		}
	}

	// UI 채움 1회
	ResultPopupInstance->ApplyResultOnce(State, MyPS, OppPS);

	// 중복 오픈 방지
	bResultPopupShown = true;

	UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][CL] ResultPopup OPENED My=%s Opp=%s"),
		*MyPS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens),
		OppPS ? *OppPS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens) : TEXT("None"));

	// (선택) 마우스 커서/UI 입력 모드
	PC->bShowMouseCursor = true;

	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
}