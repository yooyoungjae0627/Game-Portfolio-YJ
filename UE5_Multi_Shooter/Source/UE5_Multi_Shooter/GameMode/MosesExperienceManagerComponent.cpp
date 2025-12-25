#include "MosesExperienceManagerComponent.h"

#include "MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/System/MosesAssetManager.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Engine.h" // GEngine

// Plugin path 만들기
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

// ------------------------------
// OnScreen Helper
// ------------------------------
static void ScreenMsg(const FColor& Color, const FString& Msg, float Time = 8.f)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Time, Color, Msg);
	}
}

UMosesExperienceManagerComponent::UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	// (가능하면 .h에서 기본값 초기화도 추천)
	// 여기서 최소한의 안전 초기화
	LoadState = EMosesExperienceLoadState::Unloaded;
	bNotifiedReadyOnce = false;
	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;
	CurrentExperience = nullptr;
	PendingExperienceId = FPrimaryAssetId();
	CurrentExperienceId = FPrimaryAssetId();
}

void UMosesExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMosesExperienceManagerComponent, CurrentExperienceId);
}

// ------------------------------
// SERVER: set experience id (authoritative)
// ------------------------------
void UMosesExperienceManagerComponent::ServerSetCurrentExperience_Implementation(FPrimaryAssetId ExperienceId)
{
	check(GetOwner());
	check(GetOwner()->HasAuthority());

	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));

	ScreenMsg(FColor::Orange,
		FString::Printf(TEXT("[EXP-MGR][ServerSet] In=%s State=%d"), *ExperienceId.ToString(), (int32)LoadState));

	if (!ExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("ServerSetCurrentExperience: Invalid ExperienceId"));
		return;
	}

	// 타입 교정 (혹시 다른 타입으로 들어오면 Experience로 맞춤)
	if (ExperienceId.PrimaryAssetType != ExperienceType)
	{
		ScreenMsg(FColor::Orange,
			FString::Printf(TEXT("[EXP-MGR][ServerSet] FixType -> Experience:%s"), *ExperienceId.PrimaryAssetName.ToString()));
		ExperienceId = FPrimaryAssetId(ExperienceType, ExperienceId.PrimaryAssetName);
	}

	// 중복 방지
	if (CurrentExperienceId == ExperienceId && LoadState != EMosesExperienceLoadState::Unloaded)
	{
		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][ServerSet] Ignore duplicate. Id=%s State=%d"),
				*ExperienceId.ToString(), (int32)LoadState));
		return;
	}

	ResetExperienceLoadState(); // ✅ 콜백 리스트는 지우면 안 됨(아래 구현 참고)
	CurrentExperienceId = ExperienceId;

	UE_LOG(LogMosesExp, Log, TEXT("[SERVER] Chose ExperienceId=%s"), *CurrentExperienceId.ToString());
	ScreenMsg(FColor::Orange, FString::Printf(TEXT("[EXP-MGR][ServerSet] Chosen=%s"), *CurrentExperienceId.ToString()));

	// 서버도 클라와 동일 로딩 경로를 타게 한다
	OnRep_CurrentExperienceId();
}

// ------------------------------
// Register callbacks
// ------------------------------
void UMosesExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate&& Delegate)
{
	// 이미 로딩 완료면 즉시 실행
	if (IsExperienceLoaded())
	{
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][Register] Already READY -> ExecuteImmediately"));
		Delegate.ExecuteIfBound(CurrentExperience);
		return;
	}

	// 로딩 전/중이면 콜백 등록
	OnExperienceLoadedCallbacks.Add(MoveTemp(Delegate));
	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][Register] Added callback. Count=%d"), OnExperienceLoadedCallbacks.Num()));
}

const UMosesExperienceDefinition* UMosesExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(IsExperienceLoaded());
	check(CurrentExperience);
	return CurrentExperience;
}

// ------------------------------
// RepNotify: start loading
// ------------------------------
void UMosesExperienceManagerComponent::OnRep_CurrentExperienceId()
{
	ScreenMsg(FColor::Yellow,
		FString::Printf(TEXT("[EXP-MGR][OnRep] CurrentId=%s State=%d"),
			*CurrentExperienceId.ToString(), (int32)LoadState));

	if (!CurrentExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("OnRep_CurrentExperienceId: Invalid CurrentExperienceId"));
		return;
	}

	// 중복 진입 방지
	if (LoadState != EMosesExperienceLoadState::Unloaded)
	{
		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][OnRep] Ignore (State != Unloaded). State=%d"), (int32)LoadState));
		return;
	}

	PendingExperienceId = CurrentExperienceId;
	LoadState = EMosesExperienceLoadState::Loading;

	UE_LOG(LogMosesExp, Log, TEXT("[EXP] Begin Load=%s"), *CurrentExperienceId.ToString());
	ScreenMsg(FColor::Yellow, FString::Printf(TEXT("[EXP-MGR][Load] Begin %s"), *CurrentExperienceId.ToString()));

	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	TArray<FPrimaryAssetId> AssetIds;
	AssetIds.Add(CurrentExperienceId);

	// ExperienceDefinition만 먼저 로딩 (bundle 미사용)
	AssetManager.LoadPrimaryAssets(
		AssetIds,
		{},
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceAssetsLoaded)
	);
}

// ------------------------------
// ExperienceDefinition loaded
// ------------------------------
void UMosesExperienceManagerComponent::OnExperienceAssetsLoaded()
{
	// stale callback 방지
	if (PendingExperienceId != CurrentExperienceId)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP] AssetsLoaded stale. Pending=%s Current=%s"),
			*PendingExperienceId.ToString(), *CurrentExperienceId.ToString());

		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][AssetsLoaded] STALE Pending=%s Current=%s"),
				*PendingExperienceId.ToString(), *CurrentExperienceId.ToString()));
		return;
	}

	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	const UMosesExperienceDefinition* ExperienceDef =
		AssetManager.GetPrimaryAssetObject<UMosesExperienceDefinition>(CurrentExperienceId);

	if (!ExperienceDef)
	{
		FailExperienceLoad(TEXT("OnExperienceAssetsLoaded: ExperienceDef null"));
		return;
	}

	CurrentExperience = ExperienceDef;

	UE_LOG(LogMosesExp, Log, TEXT("[EXP] Loaded ExperienceDefinition=%s"), *CurrentExperienceId.ToString());
	ScreenMsg(FColor::Yellow, FString::Printf(TEXT("[EXP-MGR][AssetsLoaded] OK %s"), *CurrentExperienceId.ToString()));

	StartLoadGameFeatures();
}

// ------------------------------
// GameFeature URL
// ------------------------------
FString UMosesExperienceManagerComponent::MakeGameFeaturePluginURL(const FString& PluginName)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		return FString();
	}

	FString DescriptorFile = Plugin->GetDescriptorFileName();
	FPaths::NormalizeFilename(DescriptorFile);

	return FString::Printf(TEXT("file:%s"), *DescriptorFile);
}

// ------------------------------
// Activate GameFeatures
// ------------------------------
void UMosesExperienceManagerComponent::StartLoadGameFeatures()
{
	check(CurrentExperience);

	LoadState = EMosesExperienceLoadState::LoadingGameFeatures;

	const TArray<FString>& Features = CurrentExperience->GameFeaturesToEnable;

	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][GF] Start. Num=%d"), Features.Num()));

	// GF가 없으면 바로 READY
	if (Features.Num() == 0)
	{
		UE_LOG(LogMosesExp, Log, TEXT("[GF] None -> READY"));
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][GF] None -> READY"));
		OnExperienceFullLoadCompleted();
		return;
	}

	PendingGFCount = Features.Num();
	CompletedGFCount = 0;
	bAnyGFFailed = false;

	UGameFeaturesSubsystem& GFS = UGameFeaturesSubsystem::Get();

	UE_LOG(LogMosesExp, Warning, TEXT("[GF] Enabling %d features..."), PendingGFCount);
	ScreenMsg(FColor::Cyan, FString::Printf(TEXT("[EXP-MGR][GF] Enabling %d..."), PendingGFCount));

	for (const FString& PluginName : Features)
	{
		const FString URL = MakeGameFeaturePluginURL(PluginName);

		UE_LOG(LogMosesExp, Warning, TEXT("[GF] Request Activate: %s URL=%s"), *PluginName, *URL);
		ScreenMsg(FColor::Cyan, FString::Printf(TEXT("[EXP-MGR][GF] Req %s"), *PluginName));

		if (URL.IsEmpty())
		{
			// URL 생성 실패 = 플러그인 못 찾음
			bAnyGFFailed = true;
			CompletedGFCount++;
			ScreenMsg(FColor::Red, FString::Printf(TEXT("[EXP-MGR][GF] URL EMPTY %s"), *PluginName));
			continue;
		}

		GFS.LoadAndActivateGameFeaturePlugin(
			URL,
			FGameFeaturePluginLoadComplete::CreateUObject(
				this,
				&ThisClass::OnOneGameFeatureActivated,
				PluginName
			)
		);
	}
}

void UMosesExperienceManagerComponent::OnOneGameFeatureActivated(const UE::GameFeatures::FResult& /*Result*/, FString PluginName)
{
	CompletedGFCount++;

	UE_LOG(LogMosesExp, Warning, TEXT("[GF] Activate Completed Callback: %s (%d/%d)"),
		*PluginName, CompletedGFCount, PendingGFCount);

	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][GF] Done %s (%d/%d)"),
			*PluginName, CompletedGFCount, PendingGFCount));

	if (CompletedGFCount >= PendingGFCount)
	{
		if (bAnyGFFailed)
		{
			FailExperienceLoad(TEXT("One or more GameFeatures had invalid URL"));
			return;
		}

		UE_LOG(LogMosesExp, Warning, TEXT("[GF] Callbacks all arrived -> READY"));
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][GF] All callbacks -> READY"));

		OnExperienceFullLoadCompleted();
	}
}

// ------------------------------
// READY
// ------------------------------
void UMosesExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	if (LoadState == EMosesExperienceLoadState::Failed)
	{
		return;
	}

	LoadState = EMosesExperienceLoadState::Loaded;

	// READY 1회만
	if (!bNotifiedReadyOnce)
	{
		bNotifiedReadyOnce = true;

		UE_LOG(LogMosesExp, Log, TEXT("✅ READY: %s"), *CurrentExperienceId.ToString());
		ScreenMsg(FColor::Green, FString::Printf(TEXT("[EXP-MGR][READY] %s"), *CurrentExperienceId.ToString()));

		// 멀티캐스트(있으면)
		OnExperienceLoaded.Broadcast(CurrentExperience);

		// ✅ CallOrRegister로 모아둔 콜백 실행
		ScreenMsg(FColor::Green,
			FString::Printf(TEXT("[EXP-MGR][READY] ExecCallbacks Count=%d"), OnExperienceLoadedCallbacks.Num()));

		for (FMosesExperienceLoadedDelegate& D : OnExperienceLoadedCallbacks)
		{
			D.ExecuteIfBound(CurrentExperience);
		}
		OnExperienceLoadedCallbacks.Reset();
	}

	DebugDump();
}

// ------------------------------
// Reset / Fail / Debug
// ------------------------------
void UMosesExperienceManagerComponent::ResetExperienceLoadState()
{
	LoadState = EMosesExperienceLoadState::Unloaded;

	CurrentExperience = nullptr;
	bNotifiedReadyOnce = false;
	PendingExperienceId = FPrimaryAssetId();

	// 🚫 절대 지우면 안 됨:
	// GameMode(InitGameState)에서 미리 등록한 콜백이 여기서 날아가면 READY가 GameMode로 절대 전달 안 됨
	// OnExperienceLoadedCallbacks.Reset();

	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;

	ScreenMsg(FColor::Silver, TEXT("[EXP-MGR][Reset] State cleared (callbacks preserved)"));
}

void UMosesExperienceManagerComponent::FailExperienceLoad(const FString& Reason)
{
	LoadState = EMosesExperienceLoadState::Failed;

	UE_LOG(LogMosesExp, Error, TEXT("❌ LOAD FAILED: %s"), *Reason);
	ScreenMsg(FColor::Red, FString::Printf(TEXT("[EXP-MGR][FAIL] %s"), *Reason));

	DebugDump();
}

void UMosesExperienceManagerComponent::DebugDump() const
{
	UE_LOG(LogMosesExp, Log, TEXT("Dump ---------------------------"));
	UE_LOG(LogMosesExp, Log, TEXT("  State: %d"), (int32)LoadState);
	UE_LOG(LogMosesExp, Log, TEXT("  ExperienceId: %s"), *CurrentExperienceId.ToString());
	UE_LOG(LogMosesExp, Log, TEXT("  PendingId: %s"), *PendingExperienceId.ToString());
	UE_LOG(LogMosesExp, Log, TEXT("  Experience: %s"), CurrentExperience ? *CurrentExperience->GetName() : TEXT("null"));
	UE_LOG(LogMosesExp, Log, TEXT("  GF: Pending=%d Completed=%d AnyFailed=%d"),
		PendingGFCount, CompletedGFCount, bAnyGFFailed ? 1 : 0);
	UE_LOG(LogMosesExp, Log, TEXT("----------------------------------------"));
}
