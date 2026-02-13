// ============================================================================
// UE5_Multi_Shooter/System/MosesStartFlowSubsystem.cpp  (FULL - REORDERED)
// ============================================================================

#include "UE5_Multi_Shooter/System/MosesStartFlowSubsystem.h"

#include "UE5_Multi_Shooter/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UMosesStartFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 맵 로드가 끝났을 때 호출(PIE/게임 모두)
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	// 초기 바인드 시도 (Start 맵 진입 직후 등)
	TryBindExperienceReady();
}

void UMosesStartFlowSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}

	if (StartWidget)
	{
		StartWidget->RemoveFromParent();
		StartWidget = nullptr;
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Deinitialize();
}

// ============================================================================
// Map lifecycle
// ============================================================================

void UMosesStartFlowSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		return;
	}

	// 에디터 프리뷰 같은 월드는 제외
	if (LoadedWorld->WorldType != EWorldType::PIE && LoadedWorld->WorldType != EWorldType::Game)
	{
		return;
	}

	// 내 Subsystem이 바라보는 월드와 일치하는 경우만 처리
	if (GetWorld() != LoadedWorld)
	{
		return;
	}

	TryBindExperienceReady();
}

// ============================================================================
// Experience binding
// ============================================================================

void UMosesStartFlowSubsystem::TryBindExperienceReady()
{
	if (UMosesExperienceManagerComponent* ExpMgr = FindExperienceManager())
	{
		ExpMgr->CallOrRegister_OnExperienceLoaded(
			FMosesExperienceLoadedDelegate::CreateUObject(this, &ThisClass::HandleExperienceReady)
		);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
		}
		return;
	}

	// GameState/ExpMgr가 아직 생성되지 않았다면 짧게 재시도
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&ThisClass::TryBindExperienceReady,
			0.2f,
			true
		);
	}
}

void UMosesStartFlowSubsystem::HandleExperienceReady(const UMosesExperienceDefinition* Experience)
{
	UWorld* World = GetWorld();
	if (!World || !Experience)
	{
		return;
	}

	// Start 맵에서만 Start UI 표시
	const FString MapNameLower = World->GetMapName().ToLower();
	if (!MapNameLower.Contains(TEXT("start")))
	{
		return;
	}

	ShowStartUIFromExperience(Experience);
}

// ============================================================================
// UI
// ============================================================================

void UMosesStartFlowSubsystem::ShowStartUIFromExperience(const UMosesExperienceDefinition* Experience)
{
	if (!Experience || StartWidget)
	{
		return;
	}

	APlayerController* PC = GetLocalPlayerController();
	if (!PC)
	{
		return;
	}

	const TSoftClassPtr<UUserWidget> StartWidgetClass = Experience->GetStartWidgetClass();
	if (StartWidgetClass.IsNull())
	{
		return;
	}

	UClass* WidgetClass = StartWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		return;
	}

	StartWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!StartWidget)
	{
		return;
	}

	StartWidget->AddToViewport(10);

	PC->bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}

// ============================================================================
// Helpers
// ============================================================================

UMosesExperienceManagerComponent* UMosesStartFlowSubsystem::FindExperienceManager() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return nullptr;
	}

	return GS->FindComponentByClass<UMosesExperienceManagerComponent>();
}

APlayerController* UMosesStartFlowSubsystem::GetLocalPlayerController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return UGameplayStatics::GetPlayerController(World, 0);
}
