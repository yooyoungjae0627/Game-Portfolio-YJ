#include "GameFeatureAction_MosesCombatUI.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMosesGFUI, Log, All);

UGameFeatureAction_MosesCombatUI::UGameFeatureAction_MosesCombatUI()
{
}

void UGameFeatureAction_MosesCombatUI::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	bIsActive = true;

	UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Activating Action=%s"), *GetNameSafe(this));

	if (!PostWorldInitHandle.IsValid())
	{
		PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
			this, &UGameFeatureAction_MosesCombatUI::HandlePostWorldInitialization);
	}

	if (!WorldCleanupHandle.IsValid())
	{
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
			this, &UGameFeatureAction_MosesCombatUI::HandleWorldCleanup);
	}

	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			InjectForWorld(WorldContext.World());
		}
	}
}

void UGameFeatureAction_MosesCombatUI::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Deactivating Action=%s"), *GetNameSafe(this));

	bIsActive = false;

	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			RemoveForWorld(WorldContext.World());
		}
	}

	if (PostWorldInitHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
		PostWorldInitHandle.Reset();
	}

	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}

	WorldDataByWorld.Reset();
}

void UGameFeatureAction_MosesCombatUI::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	InjectForWorld(World);
}

void UGameFeatureAction_MosesCombatUI::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	RemoveForWorld(World);
}

bool UGameFeatureAction_MosesCombatUI::ShouldInjectForWorld(const UWorld* World) const
{
	if (!bIsActive || !World)
	{
		return false;
	}

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const FString WorldName = World->GetName();
	if (!WorldName.Contains(MatchWorldNameToken))
	{
		return false;
	}

	return true;
}

void UGameFeatureAction_MosesCombatUI::InjectForWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (!ShouldInjectForWorld(World))
	{
		UE_LOG(LogMosesGFUI, Verbose, TEXT("[GF_UI] Skip World=%s NetMode=%d Token=%s"),
			*GetNameSafe(World),
			(int32)World->GetNetMode(),
			*MatchWorldNameToken);
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogMosesGFUI, Verbose, TEXT("[GF_UI] Skip: No GameInstance World=%s"), *GetNameSafe(World));
		return;
	}

	if (HUDWidgetClass.IsNull())
	{
		UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Skip: HUDWidgetClass is NULL (Set in GameFeatureData Action)"));
		return;
	}

	UClass* WidgetClass = HUDWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Skip: HUDWidgetClass LoadSynchronous failed Path=%s"),
			*HUDWidgetClass.ToString());
		return;
	}

	FPerWorldData& PerWorldData = WorldDataByWorld.FindOrAdd(World);

	const TArray<ULocalPlayer*>& LocalPlayers = GameInstance->GetLocalPlayers();
	if (LocalPlayers.Num() <= 0)
	{
		UE_LOG(LogMosesGFUI, Verbose, TEXT("[GF_UI] Skip: No LocalPlayers World=%s"), *GetNameSafe(World));
		return;
	}

	bool bAnyPCReady = false;

	for (ULocalPlayer* LocalPlayer : LocalPlayers)
	{
		if (!LocalPlayer)
		{
			continue;
		}

		APlayerController* PC = LocalPlayer->GetPlayerController(World);
		if (!PC)
		{
			continue;
		}

		bAnyPCReady = true;

		if (const TWeakObjectPtr<UUserWidget>* ExistingPtr = PerWorldData.WidgetByLocalPlayer.Find(LocalPlayer))
		{
			if (ExistingPtr->IsValid())
			{
				continue;
			}
		}

		UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
		if (!Widget)
		{
			UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] CreateWidget failed PC=%s Class=%s"),
				*GetNameSafe(PC),
				*GetNameSafe(WidgetClass));
			continue;
		}

		Widget->AddToViewport(ViewportZOrder);
		PerWorldData.WidgetByLocalPlayer.Add(LocalPlayer, Widget);

		UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] HUD Attached World=%s PC=%s Widget=%s Z=%d"),
			*GetNameSafe(World),
			*GetNameSafe(PC),
			*GetNameSafe(Widget),
			ViewportZOrder);
	}

	if (!bAnyPCReady)
	{
		UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] No PC yet -> ScheduleRetry World=%s"), *GetNameSafe(World));
		ScheduleRetryInject(World);
	}
}

void UGameFeatureAction_MosesCombatUI::RemoveForWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	FPerWorldData* PerWorldData = WorldDataByWorld.Find(World);
	if (!PerWorldData)
	{
		return;
	}

	if (PerWorldData->RetryTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PerWorldData->RetryTimerHandle);
		PerWorldData->RetryTimerHandle.Invalidate();
	}

	for (TPair<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<UUserWidget>>& Pair : PerWorldData->WidgetByLocalPlayer)
	{
		if (UUserWidget* Widget = Pair.Value.Get())
		{
			const bool bWasInViewport = Widget->IsInViewport();
			Widget->RemoveFromParent();

			UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] HUD Removed World=%s Widget=%s WasInViewport=%d"),
				*GetNameSafe(World),
				*GetNameSafe(Widget),
				bWasInViewport ? 1 : 0);
		}
	}

	PerWorldData->WidgetByLocalPlayer.Reset();
	WorldDataByWorld.Remove(World);
}

void UGameFeatureAction_MosesCombatUI::ScheduleRetryInject(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (!ShouldInjectForWorld(World))
	{
		return;
	}

	FPerWorldData& PerWorldData = WorldDataByWorld.FindOrAdd(World);

	if (PerWorldData.RetryTimerHandle.IsValid())
	{
		return;
	}

	if (PerWorldData.RetryCount >= MaxRetryCount)
	{
		UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Retry GIVEUP World=%s Count=%d"), *GetNameSafe(World), PerWorldData.RetryCount);
		return;
	}

	PerWorldData.RetryCount++;

	World->GetTimerManager().SetTimer(
		PerWorldData.RetryTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGameFeatureAction_MosesCombatUI::RetryInject, World),
		RetryIntervalSeconds,
		false);

	UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Retry Scheduled World=%s Try=%d/%d Interval=%.2f"),
		*GetNameSafe(World),
		PerWorldData.RetryCount,
		MaxRetryCount,
		RetryIntervalSeconds);
}

void UGameFeatureAction_MosesCombatUI::RetryInject(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (FPerWorldData* PerWorldData = WorldDataByWorld.Find(World))
	{
		PerWorldData->RetryTimerHandle.Invalidate();
	}

	InjectForWorld(World);
}
