#include "MosesStartFlowSubsystem.h"
#include "MosesUserSessionSubsystem.h"

#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "UObject/UObjectGlobals.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UMosesStartFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Experience READY에 바인딩 시도 (GS가 아직 없을 수 있어서 재시도 구조)
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

	Super::Deinitialize();
}

void UMosesStartFlowSubsystem::SubmitNicknameAndEnterLobby(const FString& Nickname)
{
	if (UGameInstance* GI = UMosesGameInstance::Get(this))
	{
		if (UMosesUserSessionSubsystem* Session = GI->GetSubsystem<UMosesUserSessionSubsystem>())
		{
			Session->SetNickname(Nickname);

			// 네 환경에 맞게 변경
			// - 로컬 테스트: OpenLevel로 바꾸거나
			// - DS 접속: 127.0.0.1:7777 유지
			const FString LobbyAddr = TEXT("127.0.0.1:7777");
			Session->TravelToLobby(LobbyAddr, FString::Printf(TEXT("Nick=%s"), *Nickname));
		}
	}
}

void UMosesStartFlowSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		return;
	}

	// PIE/Game 월드에서만
	if (LoadedWorld->WorldType != EWorldType::PIE && LoadedWorld->WorldType != EWorldType::Game)
	{
		return;
	}

	// 이 Subsystem의 월드와 같은 경우만
	if (GetWorld() != LoadedWorld)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[StartFlow] PostLoadMapWithWorld -> Rebind. World=%s"), *GetNameSafe(LoadedWorld));
	TryBindExperienceReady();
}

void UMosesStartFlowSubsystem::TryBindExperienceReady()
{
	UE_LOG(LogTemp, Warning, TEXT("[StartFlow] TryBindExperienceReady GS=%s"),
		*GetNameSafe(GetWorld() ? GetWorld()->GetGameState() : nullptr));

	if (UMosesExperienceManagerComponent* ExpMgr = FindExperienceManager())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartFlow] ExpMgr=%s"), *GetNameSafe(ExpMgr));

		// READY면 즉시 호출, 아니면 READY 순간 1회 호출
		ExpMgr->CallOrRegister_OnExperienceLoaded(
			FMosesExperienceLoadedDelegate::CreateUObject(this, &ThisClass::HandleExperienceReady)
		);

		// 바인딩 성공했으면 재시도 타이머는 끈다
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
		}
		return;
	}

	// GameState가 아직 없거나 컴포넌트가 아직 붙기 전이면 재시도
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
	// Start 맵/Start Experience에서만 띄우는 안전 가드
	UWorld* World = GetWorld();
	if (!World || !Experience)
	{
		return;
	}

	const FString MapNameLower = World->GetMapName().ToLower();
	const bool bIsStartMap = MapNameLower.Contains(TEXT("start"));

	if (!bIsStartMap)
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

	UClass* WidgetClass = Experience->StartWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	StartWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!StartWidget)
	{
		return;
	}

	StartWidget->AddToViewport();

	PC->bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(StartWidget->TakeWidget()); // ✅ 포커스까지 주면 안정적
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
