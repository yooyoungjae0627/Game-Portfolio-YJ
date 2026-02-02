// ============================================================================
// UE5_Multi_Shooter/UI/Match/MosesMatchHUD.cpp  (FULL - UPDATED)  [STEP3]
// ============================================================================
//
// [STEP3 핵심 변경점]
// - HUD가 PlayerState 바인딩 후, CombatComponent(SSOT) Delegate도 추가로 바인딩한다.
// - CombatComponent -> OnAmmoChanged는 CurrentSlot 변경(OnRep_CurrentSlot)에서도 즉시 발생하므로,
//   1~4 스왑 시 HUD 탄약이 즉시 전환된다.
// - HUD는 "현재 무기 탄약만" 표시하므로, 전달되는 Mag/Reserve를 그대로 표시하면 된다.
//
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesMatchHUD.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Weapon/MosesWeaponData.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"

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

	// 2) 초기값 세팅
	RefreshInitial();

	// 3) 바인딩이 늦게 되는 케이스 재시도
	StartBindRetry();

	// 4) Crosshair Update Loop (표시 전용)
	StartCrosshairUpdate();

	// 5) ScopeWidget 초기 숨김
	SetScopeVisible_Local(false);
}

void UMosesMatchHUD::NativeDestruct()
{
	StopBindRetry();
	StopCrosshairUpdate();

	// [STEP3] Combat 먼저 해제(PS 해제 전에)
	UnbindCombatComponent();

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

bool UMosesMatchHUD::IsCombatComponentBound() const
{
	return CachedCombatComponent.IsValid();
}

void UMosesMatchHUD::StartBindRetry()
{
	// PS / GS / Combat이 모두 바인딩이면 재시도 필요 없음
	if (IsPlayerStateBound() && IsMatchGameStateBound() && IsCombatComponentBound())
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

	// [STEP3] PS 바인딩이 되면 Combat도 재시도
	if (!IsCombatComponentBound())
	{
		BindToCombatComponent_FromPlayerState();
	}

	ApplySnapshotFromMatchGameState();

	const bool bDone = IsPlayerStateBound() && IsMatchGameStateBound() && IsCombatComponentBound();
	if (bDone)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry DONE Try=%d"), BindRetryTryCount);
		StopBindRetry();
		return;
	}

	if (BindRetryTryCount >= BindRetryMaxTry)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] BindRetry GIVEUP Try=%d (PS=%d GS=%d Combat=%d)"),
			BindRetryTryCount,
			IsPlayerStateBound() ? 1 : 0,
			IsMatchGameStateBound() ? 1 : 0,
			IsCombatComponentBound() ? 1 : 0);

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
		// PS는 동일하더라도 Combat이 바뀌는 경우가 있어 Combat 바인딩은 보강한다.
		BindToCombatComponent_FromPlayerState();
		return;
	}

	// 기존 PS 바인딩 제거
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

	// [STEP3] PS의 AmmoChanged는 fallback/초기값용으로 유지하되,
	//         "즉시 전환"은 CombatComponent 쪽을 우선시한다.
	PS->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged_FromPS);
	PS->OnGrenadeChanged.AddUObject(this, &ThisClass::HandleGrenadeChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound PlayerState delegates PS=%s"), *GetNameSafe(PS));

	// [STEP3] PS 바인딩 후 Combat 바인딩
	BindToCombatComponent_FromPlayerState();
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

	// ✅ 핵심: CurrentSlot 변화/탄약 변화가 가장 즉시/정확하게 들어오는 경로
	Combat->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged_FromCombat);

	// 장착 변화 시에도 "현재 무기 탄약"을 다시 한 번 강제 표시(안전망)
	Combat->OnEquippedChanged.AddUObject(this, &ThisClass::HandleEquippedChanged_FromCombat);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound CombatComponent delegates Combat=%s PS=%s"),
		*GetNameSafe(Combat), *GetNameSafe(PS));

	// 바인딩 직후 스냅샷(현재 슬롯 탄약) 반영
	HandleAmmoChanged_FromCombat(Combat->GetCurrentMagAmmo(), Combat->GetCurrentReserveAmmo());
}

void UMosesMatchHUD::UnbindCombatComponent()
{
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		Combat->OnAmmoChanged.RemoveAll(this);
		Combat->OnEquippedChanged.RemoveAll(this);

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

	// [STEP3] Combat이 바인딩되어 있으면 "현재 무기 탄약" 스냅샷도 강제 적용
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		HandleAmmoChanged_FromCombat(Combat->GetCurrentMagAmmo(), Combat->GetCurrentReserveAmmo());
	}
}

void UMosesMatchHUD::RefreshInitial()
{
	HandleScoreChanged(0);
	HandleDeathsChanged(0);

	// 초기에는 0/0으로 두고, 바인딩 완료 시 Combat/PS 이벤트로 실제 값이 들어오게 한다.
	HandleAmmoChanged_FromPS(0, 0);
	HandleGrenadeChanged(0);

	HandleHealthChanged(100.f, 100.f);
	HandleShieldChanged(100.f, 100.f);

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

	if (bIsSniper && !bScopeOn)
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (bIsSniper && bScopeOn)
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

// --------------------------------------------------------------------
// [STEP3] CombatComponent Handlers (즉시 전환 보장)
// --------------------------------------------------------------------

void UMosesMatchHUD::HandleAmmoChanged_FromCombat(int32 Mag, int32 Reserve)
{
	// ✅ 핵심: CurrentSlot 기준 탄약만 들어오는 이벤트이므로 그대로 표시하면 된다.
	if (WeaponAmmoAmount)
	{
		WeaponAmmoAmount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, Reserve)));
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL][STEP3] AmmoChanged FromCombat %d/%d"), Mag, Reserve);
}

void UMosesMatchHUD::HandleEquippedChanged_FromCombat(int32 /*SlotIndex*/, FGameplayTag /*WeaponId*/)
{
	if (UMosesCombatComponent* Combat = CachedCombatComponent.Get())
	{
		HandleAmmoChanged_FromCombat(Combat->GetCurrentMagAmmo(), Combat->GetCurrentReserveAmmo());
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
