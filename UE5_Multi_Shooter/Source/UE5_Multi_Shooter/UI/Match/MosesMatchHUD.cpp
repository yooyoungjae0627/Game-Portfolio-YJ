#include "MosesMatchHUD.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesGameState.h"

void UMosesMatchHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BindToPlayerState();
	BindToGameState();
	RefreshInitial();
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

	PS->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	PS->OnShieldChanged.AddUObject(this, &ThisClass::HandleShieldChanged);
	PS->OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	PS->OnDeathsChanged.AddUObject(this, &ThisClass::HandleDeathsChanged);
	PS->OnAmmoChanged.AddUObject(this, &ThisClass::HandleAmmoChanged);
	PS->OnGrenadeChanged.AddUObject(this, &ThisClass::HandleGrenadeChanged);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[HUD][CL] Bound PlayerState delegates PS=%s"), *GetNameSafe(PS));
}

void UMosesMatchHUD::BindToGameState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AMosesGameState* GS = World->GetGameState<AMosesGameState>();
	if (!GS)
	{
		return;
	}

	GS->OnMatchTimeChanged.AddUObject(this, &ThisClass::HandleMatchTimeChanged);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[HUD][CL] Bound GameState delegates GS=%s"), *GetNameSafe(GS));
}

void UMosesMatchHUD::RefreshInitial()
{
	// 초기값은 Delegate가 곧 들어오지만, 안전하게 기본값 세팅
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

FString UMosesMatchHUD::ToMMSS(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 MM = Clamped / 60;
	const int32 SS = Clamped % 60;
	return FString::Printf(TEXT("%02d:%02d"), MM, SS);
}
