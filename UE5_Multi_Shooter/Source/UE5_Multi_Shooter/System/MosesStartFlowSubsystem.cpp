// UE5_Multi_Shooter/System/MosesStartFlowSubsystem.cpp

#include "UE5_Multi_Shooter/System/MosesStartFlowSubsystem.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h" // [ADD] PC / InputMode
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

void UMosesStartFlowSubsystem::TryBindExperienceReady()
{
	if (UMosesExperienceManagerComponent* ExpMgr = FindExperienceManager())
	{
		// Experience READY면 즉시 호출 / 아니면 READY 순간 호출
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

	// Start 맵에서만 Start UI를 띄운다.
	// (프로젝트 정책에 맞게 "L_Start" 같은 이름으로 고정하는 걸 추천)
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

	// [ADD] PC는 로컬 플레이어에서 가져와야 한다.
	APlayerController* PC = GetLocalPlayerController();
	if (!PC)
	{
		return;
	}

	// Experience의 Start UI 클래스 가져오기 (private 직접 접근 금지)
	const TSoftClassPtr<UUserWidget> StartWidgetClass = Experience->GetStartWidgetClass(); // [MOD]
	if (StartWidgetClass.IsNull())
	{
		return;
	}

	// 동기 로드(시작 화면은 단순/확실함 우선)
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

	// UIOnly 입력 모드로 전환
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

APlayerController* UMosesStartFlowSubsystem::GetLocalPlayerController() const // [ADD]
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 멀티/PIE에서도 "로컬 플레이어 0번"을 기준으로 Start UI를 띄운다.
	return UGameplayStatics::GetPlayerController(World, 0);
}
