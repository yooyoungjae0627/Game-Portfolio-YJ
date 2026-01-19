// MosesExperienceManagerComponent.cpp

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/System/MosesAssetManager.h"

#include "GameFeaturesSubsystem.h"
#include "GameFeaturePluginOperationResult.h"

#include "Net/UnrealNetwork.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

UMosesExperienceManagerComponent::UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	LoadState = EMosesExperienceLoadState::Unloaded;
	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;

	ActivatedGFURLs.Reset();
}

void UMosesExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMosesExperienceManagerComponent, CurrentExperienceId);
}

void UMosesExperienceManagerComponent::ServerSetCurrentExperience_Implementation(FPrimaryAssetId ExperienceId)
{
	check(GetOwner());
	check(GetOwner()->HasAuthority());

	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));

	if (!ExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("ServerSetCurrentExperience: Invalid ExperienceId"));
		return;
	}

	// 실수 방지: Type을 강제 교정
	if (ExperienceId.PrimaryAssetType != ExperienceType)
	{
		ExperienceId = FPrimaryAssetId(ExperienceType, ExperienceId.PrimaryAssetName);
	}

	// 동일 Experience 중복 실행 방지
	if (CurrentExperienceId == ExperienceId && LoadState != EMosesExperienceLoadState::Unloaded)
	{
		return;
	}

	// Experience 교체 시작(이전 GF 내리기 포함)
	ResetForNewExperience();

	CurrentExperienceId = ExperienceId;

	// 서버도 클라와 같은 루트로 로딩시키기 위해 OnRep 직접 호출
	OnRep_CurrentExperienceId();
}

void UMosesExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.ExecuteIfBound(CurrentExperience);
		return;
	}

	PendingReadyCallbacks.Add(Delegate);
}

const UMosesExperienceDefinition* UMosesExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(IsExperienceLoaded());
	check(CurrentExperience);
	return CurrentExperience;
}

void UMosesExperienceManagerComponent::OnRep_CurrentExperienceId()
{
	if (!CurrentExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("OnRep_CurrentExperienceId: Invalid Id"));
		return;
	}

	// 핵심: 전환 지원
	if (LoadState != EMosesExperienceLoadState::Unloaded)
	{
		ResetForNewExperience();
	}

	PendingExperienceId = CurrentExperienceId;
	StartLoadExperienceAssets();
}

void UMosesExperienceManagerComponent::StartLoadExperienceAssets()
{
	LoadState = EMosesExperienceLoadState::LoadingAssets;

	// PrimaryAsset 로드(서버/클라 동일)
	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	TArray<FPrimaryAssetId> AssetIds;
	AssetIds.Add(CurrentExperienceId);

	AssetManager.LoadPrimaryAssets(
		AssetIds,
		{},
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceAssetsLoaded)
	);
}

void UMosesExperienceManagerComponent::OnExperienceAssetsLoaded()
{
	// stale 방지: 로딩 중 Experience가 바뀐 경우 무시
	if (PendingExperienceId != CurrentExperienceId)
	{
		return;
	}

	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();
	const UMosesExperienceDefinition* LoadedDef =
		AssetManager.GetPrimaryAssetObject<UMosesExperienceDefinition>(CurrentExperienceId);

	if (!LoadedDef)
	{
		FailExperienceLoad(TEXT("OnExperienceAssetsLoaded: ExperienceDefinition null"));
		return;
	}

	CurrentExperience = LoadedDef;
	StartLoadGameFeatures();
}

FString UMosesExperienceManagerComponent::MakeGameFeaturePluginURL(const FString& PluginName)
{
	/**
	 * 개발자 주석:
	 * - GameFeaturesSubsystem이 요구하는 표준 URL 포맷: "file:<Descriptor.uplugin 경로>"
	 * - PluginName만으로 실제 파일 경로를 얻기 위해 IPluginManager를 사용한다.
	 */
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		return FString();
	}

	FString DescriptorFile = Plugin->GetDescriptorFileName();
	FPaths::NormalizeFilename(DescriptorFile);

	return FString::Printf(TEXT("file:%s"), *DescriptorFile);
}

void UMosesExperienceManagerComponent::StartLoadGameFeatures()
{
	check(CurrentExperience);

	LoadState = EMosesExperienceLoadState::LoadingGameFeatures;
	ActivatedGFURLs.Reset();

	const TArray<FString>& Features = CurrentExperience->GetGameFeaturesToEnable();
	if (Features.Num() == 0)
	{
		// GF가 없는 Experience도 가능(단순 페이즈 등)
		FinishExperienceLoad();
		return;
	}

	PendingGFCount = Features.Num();
	CompletedGFCount = 0;
	bAnyGFFailed = false;
	LastGFFailReason.Reset();

	UGameFeaturesSubsystem& GFS = UGameFeaturesSubsystem::Get();

	for (const FString& PluginName : Features)
	{
		const FString URL = MakeGameFeaturePluginURL(PluginName);
		if (URL.IsEmpty())
		{
			bAnyGFFailed = true;
			CompletedGFCount++;
			continue;
		}

		// 비동기 Load+Activate 요청
		GFS.LoadAndActivateGameFeaturePlugin(
			URL,
			FGameFeaturePluginLoadComplete::CreateUObject(this, &ThisClass::OnOneGameFeatureActivated, PluginName)
		);
	}
}

void UMosesExperienceManagerComponent::OnOneGameFeatureActivated(const UE::GameFeatures::FResult& Result, FString PluginName)
{
	CompletedGFCount++;

	if (Result.HasError())
	{
		bAnyGFFailed = true;
		LastGFFailReason = Result.GetError();
	}
	else
	{
		// 성공한 GF는 나중에 전환 시 내리기 위해 URL 기록
		const FString URL = MakeGameFeaturePluginURL(PluginName);
		if (!URL.IsEmpty())
		{
			ActivatedGFURLs.AddUnique(URL);
		}
	}

	if (CompletedGFCount >= PendingGFCount)
	{
		if (bAnyGFFailed)
		{
			FailExperienceLoad(FString::Printf(TEXT("GameFeature activation failed. Last=%s"), *LastGFFailReason));
			return;
		}

		FinishExperienceLoad();
	}
}

void UMosesExperienceManagerComponent::FinishExperienceLoad()
{
	LoadState = EMosesExperienceLoadState::Loaded;

	// “Experience READY” 브로드캐스트
	OnExperienceLoaded.Broadcast(CurrentExperience);

	// 등록된 대기 콜백 실행
	for (FMosesExperienceLoadedDelegate& CB : PendingReadyCallbacks)
	{
		CB.ExecuteIfBound(CurrentExperience);
	}
	PendingReadyCallbacks.Reset();
}

void UMosesExperienceManagerComponent::FailExperienceLoad(const FString& Reason)
{
	// 실패 시에도 이전 GF가 남아있으면 위험하므로 정리
	LoadState = EMosesExperienceLoadState::Failed;
	DeactivatePreviouslyActivatedGameFeatures();

	// 포트폴리오에서는 로그 채널을 붙이면 더 좋다(여기서는 단순화).
	(void)Reason;
}

void UMosesExperienceManagerComponent::ResetForNewExperience()
{
	// Experience 전환 핵심: 이전 GF 내리기
	DeactivatePreviouslyActivatedGameFeatures();

	LoadState = EMosesExperienceLoadState::Unloaded;
	CurrentExperience = nullptr;
	PendingExperienceId = FPrimaryAssetId();

	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;
	LastGFFailReason.Reset();
	PendingReadyCallbacks.Reset();
}

void UMosesExperienceManagerComponent::DeactivatePreviouslyActivatedGameFeatures()
{
	if (ActivatedGFURLs.Num() == 0)
	{
		return;
	}

	UGameFeaturesSubsystem& GFS = UGameFeaturesSubsystem::Get();

	for (const FString& URL : ActivatedGFURLs)
	{
		/**
		 * 개발자 주석:
		 * - Experience 전환 시 "이전 Experience가 켠 GameFeature"를 내려야 한다.
		 * - Deactivate(기능 OFF) + Unload(메모리에서 내림)까지 해주면
		 *   다음 Experience가 올리는 기능만 남아 “조립형”이 완성된다.
		 *
		 * 주의:
		 * - 내부적으로 비동기/상태머신이므로, 포트폴리오 단계에서는
		 *   "전환 직후 즉시 새 Experience 로딩"을 해도 대부분 문제 없게 설계한다.
		 * - 더 엄밀히 하려면 Deactivate 완료 콜백까지 기다리고 Unload 하는 방식으로 확장 가능.
		 */
		GFS.DeactivateGameFeaturePlugin(URL);
		GFS.UnloadGameFeaturePlugin(URL);
	}

	ActivatedGFURLs.Reset();
}
