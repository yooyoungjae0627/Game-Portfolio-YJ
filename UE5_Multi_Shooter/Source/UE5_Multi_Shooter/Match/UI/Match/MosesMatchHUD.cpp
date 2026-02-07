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
#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
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
// [MOD] Aim UI immediate sync
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

	// Crosshair: Sniper면 항상 숨김
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(bIsSniper ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// Scope: (Sniper + ScopeOn)일 때만 표시
	if (ScopeWidget)
	{
		const bool bShowScope = (bIsSniper && bScopeOn);
		ScopeWidget->SetScopeVisible(bShowScope);
	}

	UE_LOG(LogMosesHUD, VeryVerbose, TEXT("[HUD][CL][AIM] UpdateAimWidgets Sniper=%d ScopeOn=%d Crosshair=%s Scope=%s"),
		bIsSniper ? 1 : 0,
		bScopeOn ? 1 : 0,
		*GetNameSafe(CrosshairWidget),
		*GetNameSafe(ScopeWidget));
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

	CacheCaptureProgress_InternalWidgets();

	BindToPlayerState();
	BindToGameState_Match();

	RefreshInitial();
	StartBindRetry();
	StartCrosshairUpdate();

	// 5) ScopeWidget 초기 숨김
	SetScopeVisible_Local(false);

	BindToCaptureComponent_FromPlayerState();

	// [MOD] 초기 Aim UI 즉시 동기화
	UpdateAimWidgets_Immediate(); // [MOD]
}

void UMosesMatchHUD::NativeDestruct()
{
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
	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL][SCOPE] SetScopeVisible bVisible=%d ScopeWidget=%s"),
		bVisible ? 1 : 0,
		*GetNameSafe(ScopeWidget));

	if (ScopeWidget)
	{
		ScopeWidget->SetScopeVisible(bVisible);
	}
}

void UMosesMatchHUD::TryBroadcastCaptureSuccess_Once()
{
	if (bCaptureSuccessAnnounced_Local)
	{
		return;
	}

	AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(GetOwningPlayer());
	if (!MosesPC)
	{
		return;
	}

	bCaptureSuccessAnnounced_Local = true;
	MosesPC->Server_RequestCaptureSuccessAnnouncement();

	UE_LOG(LogMosesHUD, Warning, TEXT("[ANN][CL] Request CaptureSuccessAnnouncement -> PC=%s"), *GetNameSafe(MosesPC));
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

	if (!IsCombatComponentBound())
	{
		BindToCombatComponent_FromPlayerState();
	}

	if (!IsCaptureComponentBound())
	{
		BindToCaptureComponent_FromPlayerState();
	}

	ApplySnapshotFromMatchGameState();

	// [MOD] 바인딩 재시도 중에도 Aim UI 동기화
	UpdateAimWidgets_Immediate(); // [MOD]

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

	// 같은 PS면, 하위 컴포넌트만 재확인
	if (CachedPlayerState.Get() == PS)
	{
		BindToCombatComponent_FromPlayerState();
		BindToCaptureComponent_FromPlayerState();

		// ✅ [FIX] 같은 PS여도 스냅샷을 한번 더 강제 (Late bind / GF 재부착 대비)
		HandleHealthChanged(PS->GetHealth_Current(), PS->GetHealth_Max());
		HandleShieldChanged(PS->GetShield_Current(), PS->GetShield_Max());
		HandleScoreChanged(FMath::RoundToInt(PS->GetScore()));
		HandlePvPKillsChanged(PS->GetPvPKills());
		HandleCapturesChanged(PS->GetCaptures());
		HandleZombieKillsChanged(PS->GetZombieKills());
		HandleAmmoChanged_FromPS(0, 0); // PS->BroadcastAmmoAndGrenade()가 늦을 수 있으니 최소 방어

		return;
	}

	// 이전 PS 해제
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
	}

	CachedPlayerState = PS;

	// 델리게이트 바인딩
	PS->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	PS->OnShieldChanged.AddUObject(this, &ThisClass::HandleShieldChanged);
	PS->OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);

	PS->OnPlayerCapturesChanged.AddUObject(this, &ThisClass::HandleCapturesChanged);
	PS->OnPlayerZombieKillsChanged.AddUObject(this, &ThisClass::HandleZombieKillsChanged);
	PS->OnPlayerPvPKillsChanged.AddUObject(this, &ThisClass::HandlePvPKillsChanged);
	PS->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged_FromPS);
	PS->OnGrenadeChanged.AddUObject(this, &ThisClass::HandleGrenadeChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound PlayerState delegates PS=%s"), *GetNameSafe(PS));

	// ✅ [FIX][핵심] Bind 직후 “즉시 스냅샷” 강제 적용 (Delegate/Rep 타이밍 의존 제거)
	{
		const float CurHP = PS->GetHealth_Current();
		const float MaxHP = PS->GetHealth_Max();
		const float CurShield = PS->GetShield_Current();
		const float MaxShield = PS->GetShield_Max();

		HandleHealthChanged(CurHP, MaxHP);
		HandleShieldChanged(CurShield, MaxShield);

		HandleScoreChanged(FMath::RoundToInt(PS->GetScore()));
		HandleCapturesChanged(PS->GetCaptures());
		HandleZombieKillsChanged(PS->GetZombieKills());
		HandlePvPKillsChanged(PS->GetPvPKills());
	}

	// 하위 컴포넌트 바인딩
	BindToCombatComponent_FromPlayerState();
	BindToCaptureComponent_FromPlayerState();

	// Aim UI 즉시 동기화
	UpdateAimWidgets_Immediate();
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

	// 3파라미터 버전 바인딩 (ReserveMax 포함)
	Combat->OnAmmoChangedEx.AddUObject(this, &ThisClass::HandleAmmoChangedEx_FromCombat); 

	Combat->OnEquippedChanged.AddUObject(this, &ThisClass::HandleEquippedChanged_FromCombat);
	Combat->OnReloadingChanged.AddUObject(this, &ThisClass::HandleReloadingChanged_FromCombat);
	Combat->OnSlotsStateChanged.AddUObject(this, &ThisClass::HandleSlotsStateChanged_FromCombat);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound CombatComponent delegates Combat=%s PS=%s"),
		*GetNameSafe(Combat), *GetNameSafe(PS));

	// 초기 스냅샷
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

	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		HandleAmmoChangedEx_FromCombat(
			Combat->GetCurrentMagAmmo(),
			Combat->GetCurrentReserveAmmo(),
			Combat->GetCurrentReserveMax());
	}

	// 스냅샷 적용 뒤 Aim UI 동기화
	UpdateAimWidgets_Immediate();
}

void UMosesMatchHUD::RefreshInitial()
{
	// ✅ [FIX] 더미 값(100/100 등)을 강제로 박지 않는다.
	// - Bind 이후 스냅샷(PS/GS/Combat)으로만 초기 정합을 맞춘다.

	AMosesPlayerState* PS = CachedPlayerState.Get();
	if (PS)
	{
		HandleScoreChanged(FMath::RoundToInt(PS->GetScore()));
		HandlePvPKillsChanged(PS->GetPvPKills());
		HandleCapturesChanged(PS->GetCaptures());
		HandleZombieKillsChanged(PS->GetZombieKills());

		HandleHealthChanged(PS->GetHealth_Current(), PS->GetHealth_Max());
		HandleShieldChanged(PS->GetShield_Current(), PS->GetShield_Max());
	}
	else
	{
		// PS가 아직 없으면 최소 안전값만
		HandleScoreChanged(0);
		HandlePvPKillsChanged(0);
		HandleCapturesChanged(0);
		HandleZombieKillsChanged(0);

		HandleHealthChanged(0.f, 0.f);
		HandleShieldChanged(0.f, 0.f);
	}

	// Ammo/Grenade는 Combat snapshot 쪽이 더 정확하므로 ApplySnapshot이 우선
	ApplySnapshotFromMatchGameState();

	if (CaptureProgress)
	{
		CaptureProgress->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateAimWidgets_Immediate();
}


// ============================================================================
// Crosshair Update (표시 전용)
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
	// [MOD] 20Hz에서도 “최종 규칙”을 계속 강제 (스코프/무기 교체 타이밍 안정)
	UpdateAimWidgets_Immediate(); // [MOD]

	if (!CrosshairWidget)
	{
		return;
	}

	// 스나이퍼면 Spread 계산 자체가 의미 없으니 여기서 종료
	if (IsSniperEquipped_Local()) // [MOD]
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
			SpreadFactor,
			*WeaponId.ToString(),
			bScopeOn ? 1 : 0);
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
	// 표시 정책: "Mag | ReserveCur / ReserveMax"
	if (WeaponAmmoAmount)
	{
		WeaponAmmoAmount->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), Mag, ReserveCur)));
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] AmmoChangedEx Mag=%d Reserve=%d/%d"), Mag, ReserveCur, ReserveMax);

	UpdateCurrentWeaponHeader();
	UpdateSlotPanels_All();
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

	// [MOD] 무기 교체 순간에도 Aim UI 즉시 규칙 적용
	UpdateAimWidgets_Immediate(); // [MOD]
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
		MosesHUD_SetTextIfValid(Text_CurrentSlot, FText::FromString(TEXT("슬롯: -")));
		MosesHUD_SetVisibleIfValid(Text_Reloading, false);
		return;
	}

	const int32 CurSlot = Combat->GetCurrentSlot();
	const FGameplayTag CurWeaponId = Combat->GetEquippedWeaponId();

	// 한글 표시용 이름 매핑 (최소 안전: Slot 기준)
	FText WeaponDisplayName = FText::FromString(TEXT("-"));
	switch (CurSlot)
	{
	case 1: WeaponDisplayName = FText::FromString(TEXT("라이플")); break;
	case 2: WeaponDisplayName = FText::FromString(TEXT("샷건")); break;
	case 3: WeaponDisplayName = FText::FromString(TEXT("스나이퍼")); break;
	case 4: WeaponDisplayName = FText::FromString(TEXT("수류탄 런처")); break;
	default: break;
	}

	// (선택) 혹시 Slot이 0/미장착인데 태그는 유효한 경우 대비해 태그 기반 보조 매핑
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

	auto MakeAmmoTextLocal = [](int32 Mag, int32 Reserve) -> FText
		{
			return FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, Reserve));
		};

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

	Capture_ProgressBar = Cast<UProgressBar>(CaptureProgress->GetWidgetFromName(TEXT("Progress_Capture")));
	Capture_TextPercent = Cast<UTextBlock>(CaptureProgress->GetWidgetFromName(TEXT("Text_Percent")));
	Capture_TextWarning = Cast<UTextBlock>(CaptureProgress->GetWidgetFromName(TEXT("Text_Warning")));

	UE_LOG(LogMosesHUD, Warning, TEXT("[CAPTURE][HUD][CL] CacheInternal OK PB=%s Percent=%s Warning=%s"),
		*GetNameSafe(Capture_ProgressBar),
		*GetNameSafe(Capture_TextPercent),
		*GetNameSafe(Capture_TextWarning));

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

	UE_LOG(LogMosesHUD, Verbose, TEXT("[CAPTURE][HUD][CL] StateChanged Active=1 Pct=%.2f WarnF=%.2f Spot=%s"),
		Clamped,
		WarningFloat,
		*GetNameSafe(Spot.Get()));
}

void UMosesMatchHUD::HandlePvPKillsChanged(int32 NewPvPKills)
{
	// DefeatsAmount를 "내가 죽인 수(PvP Kills)"로 사용
	if (DefeatsAmount)
	{
		DefeatsAmount->SetText(FText::AsNumber(NewPvPKills));
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] PvPKillsChanged=%d -> DefeatsAmount Updated"), NewPvPKills);
}