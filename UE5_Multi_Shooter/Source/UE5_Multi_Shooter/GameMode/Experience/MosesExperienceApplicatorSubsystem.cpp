#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceApplicatorSubsystem.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceRegistrySubsystem.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h" 

#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

void UMosesExperienceApplicatorSubsystem::Initialize(FSubsystemCollectionBase& Collection) 
{
	Super::Initialize(Collection);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[EXP][APP][CL] Applicator Initialize LP=%s"),
		*GetNameSafe(GetLocalPlayer()));

	// SeamlessTravel/ServerTravel로 월드가 바뀌면 다시 바인딩해야 한다.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMap);

	StartRetryBindExperienceManager();
}

void UMosesExperienceApplicatorSubsystem::Deinitialize()
{
	StopRetryBindExperienceManager();
	ClearApplied();

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	bBoundToExpManager = false;
	CachedExpManager.Reset();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[EXP][APP][CL] Applicator Deinitialize LP=%s"),
		*GetNameSafe(GetLocalPlayer()));

	Super::Deinitialize();
}

void UMosesExperienceApplicatorSubsystem::StartRetryBindExperienceManager()
{
	StopRetryBindExperienceManager();

	RetryCount = 0;

	UWorld* World = GetWorld();
	if (!World)
	{
		// 월드가 아직 없으면 다음 틱에 다시
		return;
	}

	// 0.1초 간격으로 잠깐만 재시도(PIE/SeamlessTravel 대응)
	World->GetTimerManager().SetTimer(
		RetryBindTimerHandle,
		this,
		&ThisClass::TryBindExperienceManagerOnce,
		0.1f,
		true);
}

void UMosesExperienceApplicatorSubsystem::StopRetryBindExperienceManager()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryBindTimerHandle);
	}
}

void UMosesExperienceApplicatorSubsystem::TryBindExperienceManagerOnce() 
{
	RetryCount++;

	// 너무 오래 재시도하면 로그만 오염되니 상한
	if (RetryCount > 200) // 약 20초
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[EXP][APP][CL] Bind ExpManager TIMEOUT LP=%s"),
			*GetNameSafe(GetLocalPlayer()));
		StopRetryBindExperienceManager();
		return;
	}

	if (bBoundToExpManager)
	{
		StopRetryBindExperienceManager();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return;
	}

	UMosesExperienceManagerComponent* ExpMgr = GS->FindComponentByClass<UMosesExperienceManagerComponent>();
	if (!ExpMgr)
	{
		return;
	}

	// 바인딩 완료
	CachedExpManager = ExpMgr;
	bBoundToExpManager = true;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[EXP][APP][CL] Bound ExpManager OK GS=%s ExpMgr=%s"),
		*GetNameSafe(GS),
		*GetNameSafe(ExpMgr));

	// [ADD] Experience READY 이벤트 구독(이미 Loaded면 즉시 콜백 실행됨)
	ExpMgr->CallOrRegister_OnExperienceLoaded(
		FMosesExperienceLoadedDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded)
	);

	StopRetryBindExperienceManager();
}

void UMosesExperienceApplicatorSubsystem::HandleExperienceLoaded(const UMosesExperienceDefinition* Experience)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[EXP][APP][CL] Experience READY -> Apply Payload Exp=%s"),
		*GetNameSafe(Experience));

	UMosesGameInstance* GI = UMosesGameInstance::Get(this);
	if (!GI)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[EXP][APP][CL] FAIL: GameInstance NULL"));
		return;
	}

	UMosesExperienceRegistrySubsystem* Registry = GI->GetSubsystem<UMosesExperienceRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[EXP][APP][CL] FAIL: Registry NULL"));
		return;
	}

	// [ADD] 1) Registry에 현재 Experience Payload 저장
	Registry->SetFromExperience(Experience);

	// [ADD] 2) 실제 적용( HUD / IMC )
	ApplyCurrentExperiencePayload();
}

void UMosesExperienceApplicatorSubsystem::ApplyCurrentExperiencePayload()
{
	// 개발자 주석:
	// - Experience READY 이후 호출된다는 전제.
	// - “기존 적용을 지우고 → 현재 Experience 기준으로 다시 적용”이 가장 안전하다.
	ClearApplied();

	ApplyHUD();
	ApplyInputMapping();
}

void UMosesExperienceApplicatorSubsystem::ClearApplied()
{
	// HUD 제거
	if (SpawnedHUDWidget)
	{
		SpawnedHUDWidget->RemoveFromParent();
		SpawnedHUDWidget = nullptr;
	}

	// InputMapping 제거
	if (AppliedIMC)
	{
		if (UEnhancedInputLocalPlayerSubsystem* EIS =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			EIS->RemoveMappingContext(const_cast<UInputMappingContext*>(AppliedIMC.Get()));
		}

		AppliedIMC = nullptr;
		AppliedInputPriority = 0;
	}
}

void UMosesExperienceApplicatorSubsystem::ApplyHUD()
{
	UMosesGameInstance* GI = UMosesGameInstance::Get(this);
	if (!GI)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[HUD][CL] FAIL: GameInstance NULL"));
		return;
	}

	UMosesExperienceRegistrySubsystem* Registry = GI->GetSubsystem<UMosesExperienceRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[HUD][CL] FAIL: Registry NULL"));
		return;
	}

	const TSoftClassPtr<UUserWidget> HUDClassSoft = Registry->GetHUDWidgetClass();
	if (HUDClassSoft.IsNull())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[HUD][CL] SKIP: HUDWidgetClass is NULL (Exp may not have HUD)"));
		return;
	}

	UClass* HUDClass = HUDClassSoft.LoadSynchronous();
	if (!HUDClass)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[HUD][CL] FAIL: HUDClass LoadSynchronous NULL Path=%s"),
			*HUDClassSoft.ToSoftObjectPath().ToString());
		return;
	}

	// [MOD] CreateWidget는 OwningPlayer가 있으면 더 안전하다(로컬 HUD)
	APlayerController* PC = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[HUD][CL] FAIL: Owning PC NULL"));
		return;
	}

	SpawnedHUDWidget = CreateWidget<UUserWidget>(PC, HUDClass);
	if (!SpawnedHUDWidget)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[HUD][CL] FAIL: CreateWidget failed Class=%s"), *GetNameSafe(HUDClass));
		return;
	}

	SpawnedHUDWidget->AddToViewport();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[HUD][CL] AddToViewport OK Widget=%s Class=%s"),
		*GetNameSafe(SpawnedHUDWidget),
		*GetNameSafe(HUDClass)); // ✅ Day1 증거 로그
}

void UMosesExperienceApplicatorSubsystem::ApplyInputMapping()
{
	UMosesGameInstance* GI = UMosesGameInstance::Get(this);
	if (!GI)
	{
		return;
	}

	UMosesExperienceRegistrySubsystem* Registry = GI->GetSubsystem<UMosesExperienceRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const TSoftObjectPtr<UInputMappingContext> IMCSoft = Registry->GetInputMapping();
	if (IMCSoft.IsNull())
	{
		return;
	}

	UInputMappingContext* IMC = IMCSoft.LoadSynchronous();
	if (!IMC)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EIS =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!EIS)
	{
		return;
	}

	const int32 Priority = Registry->GetInputPriority();
	EIS->AddMappingContext(IMC, Priority);

	AppliedIMC = IMC;
	AppliedInputPriority = Priority;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[IMC][CL] AddMapping OK IMC=%s Priority=%d"),
		*GetNameSafe(IMC), Priority); // 증거 로그(선택)
}

void UMosesExperienceApplicatorSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// LocalPlayerSubsystem이라 클라에서만 의미 있음
	if (!GetLocalPlayer() || !LoadedWorld)
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[EXP][APP][CL] PostLoadMap -> Rebind ExpManager World=%s"),
		*GetNameSafe(LoadedWorld));

	// 월드 바뀌면 기존 바인딩은 무효가 될 수 있으므로 리셋 후 재시도
	bBoundToExpManager = false;
	CachedExpManager.Reset();

	StartRetryBindExperienceManager();
}