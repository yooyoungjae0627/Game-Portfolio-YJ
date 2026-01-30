// ============================================================================
// MosesMatchHUD.cpp
// ============================================================================

#include "UE5_Multi_Shooter/UI/Match/MosesMatchHUD.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/UI/Match/MosesMatchAnnouncementWidget.h"
#include "UE5_Multi_Shooter/UI/Match/MosesMatchRulePopupWidget.h"

UMosesMatchHUD::UMosesMatchHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesMatchHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UI는 클라 전용(DS에서는 생성되지 않는 것이 정상).
	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Init MatchHUD=%s OwningPlayer=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwningPlayer()));

	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddDynamic(this, &ThisClass::HandleRulesClicked);
	}

	BindToPlayerState();
	BindToGameState_Match();
	RefreshInitial();

	// Popup 기본 상태: 닫힘
	bRulesPopupVisible = false;
	if (RulePopupWidget)
	{
		RulePopupWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMosesMatchHUD::NativeDestruct()
{
	// Delegate 해제(레벨 이동, GF 토글, PIE 재시작 등에서 안전)
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
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] No AMosesMatchGameState World=%s"), *GetNameSafe(World));
		return;
	}

	CachedMatchGameState = GS;

	GS->OnMatchTimeChanged.AddUObject(this, &ThisClass::HandleMatchTimeChanged);
	GS->OnMatchPhaseChanged.AddUObject(this, &ThisClass::HandleMatchPhaseChanged);
	GS->OnAnnouncementChanged.AddUObject(this, &ThisClass::HandleAnnouncementChanged);

	UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] Bound MatchGameState delegates GS=%s"), *GetNameSafe(GS));

	// Tick/Binding 없이 초기 반영
	HandleMatchTimeChanged(GS->GetRemainingSeconds());
	HandleMatchPhaseChanged(GS->GetMatchPhase());
	HandleAnnouncementChanged(GS->GetAnnouncementState());
}

void UMosesMatchHUD::RefreshInitial()
{
	// "초기값"은 안전한 디폴트 표시. 실제 값은 Delegate로 즉시 덮어써진다.
	HandleMatchTimeChanged(0);
	HandleScoreChanged(0);
	HandleDeathsChanged(0);
	HandleAmmoChanged(0, 0);
	HandleGrenadeChanged(0);
	HandleHealthChanged(100.f, 100.f);
	HandleShieldChanged(100.f, 100.f);
}

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

void UMosesMatchHUD::HandleMatchTimeChanged(int32 RemainingSeconds)
{
	if (MatchCountdownText)
	{
		MatchCountdownText->SetText(FText::FromString(ToMMSS(RemainingSeconds)));
	}
}

void UMosesMatchHUD::HandleMatchPhaseChanged(EMosesMatchPhase NewPhase)
{
	UE_LOG(LogMosesPhase, Verbose, TEXT("[HUD][CL] Phase=%s"), *UEnum::GetValueAsString(NewPhase));
}

void UMosesMatchHUD::HandleAnnouncementChanged(const FMosesAnnouncementState& State)
{
	if (AnnouncementWidget)
	{
		AnnouncementWidget->UpdateAnnouncement(State);
	}
}

FString UMosesMatchHUD::ToMMSS(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 MM = Clamped / 60;
	const int32 SS = Clamped % 60;
	return FString::Printf(TEXT("%02d:%02d"), MM, SS);
}

void UMosesMatchHUD::HandleRulesClicked()
{
	// RulePopupWidget이 디자이너에 배치되어야 함 (BindWidgetOptional)
	if (!RulePopupWidget)
	{
		UE_LOG(LogMosesHUD, Warning, TEXT("[HUD][CL] RulesClicked: RulePopupWidget is NULL (Check BP instance name)"));
		return;
	}

	bRulesPopupVisible = !bRulesPopupVisible;
	if (bRulesPopupVisible)
	{
		RulePopupWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		RulePopupWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogMosesHUD, Verbose, TEXT("[HUD][CL] RulesPopup -> %s"), bRulesPopupVisible ? TEXT("OPEN") : TEXT("CLOSE"));
}
