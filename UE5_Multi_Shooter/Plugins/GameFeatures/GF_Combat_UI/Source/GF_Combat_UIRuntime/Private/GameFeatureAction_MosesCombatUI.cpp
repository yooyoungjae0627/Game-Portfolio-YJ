// ============================================================================
// GameFeatureAction_MosesCombatUI.cpp (FULL)
// ============================================================================

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
	// 기본값은 .h에서 설정
}

void UGameFeatureAction_MosesCombatUI::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	bIsActive = true;
	ㅣ
	UE_LOG(LogMosesGFUI, Warning, TEXT("[GF_UI] Activating Action=%s"), *GetNameSafe(this));

	// World lifecycle 델리게이트 등록
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

	// 이미 떠 있는 월드들에도 주입 시도 (PIE/SeamlessTravel에서 유효)
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

	// 떠 있는 월드에서 전부 제거/정리
	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			RemoveForWorld(WorldContext.World());
		}
	}

	// 델리게이트 해제
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
	// World가 생길 때마다 호출되는 것이 정상.
	// 여기서 "매치 월드"만 필터링해서 HUD를 붙이도록 한다.
	InjectForWorld(World);
}

void UGameFeatureAction_MosesCombatUI::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// 월드 종료 시점에 반드시 정리해야 SeamlessTravel/PIE에서 누수가 없다.
	RemoveForWorld(World);
}

bool UGameFeatureAction_MosesCombatUI::ShouldInjectForWorld(const UWorld* World) const
{
	if (!bIsActive || !World)
	{
		return false;
	}

	// Dedicated Server는 UI 없음
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// [MOD] MatchLevel에서만 붙이기 (Start/Lobby/Untitled 등 제외)
	// - PIE prefix 때문에 Contains 사용
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

	// LocalPlayer가 하나도 없다면(서버 월드 등) 스킵
	const TArray<ULocalPlayer*>& LocalPlayers = GameInstance->GetLocalPlayers();
	if (LocalPlayers.Num() <= 0)
	{
		UE_LOG(LogMosesGFUI, Verbose, TEXT("[GF_UI] Skip: No LocalPlayers World=%s"), *GetNameSafe(World));
		return;
	}

	// 핵심: LocalPlayer별로 HUD 1개 보장
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
			// 타이밍 이슈: 월드 생성 직후에는 PC가 아직 없을 수 있다.
			continue;
		}

		bAnyPCReady = true;

		// 이미 이 LocalPlayer에 HUD가 붙어있으면 중복 생성 방지
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

	// PC가 아직 준비되지 않았으면 재시도 예약
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

	// 재시도 타이머 정리
	if (PerWorldData->RetryTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PerWorldData->RetryTimerHandle);
		PerWorldData->RetryTimerHandle.Invalidate();
	}

	// 생성된 HUD 위젯 제거
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

	// 이미 타이머가 예약되어 있으면 중복 예약 방지
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

	// 타이머는 단발이므로 여기서 Invalidate (다음 예약 가능하게)
	if (FPerWorldData* PerWorldData = WorldDataByWorld.Find(World))
	{
		PerWorldData->RetryTimerHandle.Invalidate();
	}

	InjectForWorld(World);
}
