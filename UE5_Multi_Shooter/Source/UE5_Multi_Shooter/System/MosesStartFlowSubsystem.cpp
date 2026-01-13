#include "MosesStartFlowSubsystem.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UMosesStartFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

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

void UMosesStartFlowSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		return;
	}

	if (LoadedWorld->WorldType != EWorldType::PIE && LoadedWorld->WorldType != EWorldType::Game)
	{
		return;
	}

	if (GetWorld() != LoadedWorld)
	{
		return;
	}

	TryBindExperienceReady();
}

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

	// Start 맵에서만 Start UI
	const FString MapNameLower = World->GetMapName().ToLower();
	if (!MapNameLower.Contains(TEXT("start")))
	{
		return;
	}

	ShowStartUIFromExperience(Experience);
}

void UMosesStartFlowSubsystem::ShowStartUIFromExperience(const UMosesExperienceDefinition* Experience)
{
	if (!Experience || StartWidget)
	{
		return;
	}

	if (Experience->StartWidgetClass.IsNull())
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UClass* WidgetClass = Experience->StartWidgetClass.LoadSynchronous();
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
