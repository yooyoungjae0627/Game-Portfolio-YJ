
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/System/MosesAssetManager.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

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
		UE_LOG(LogMosesExp, Verbose, TEXT("[EXP][SV] Ignore duplicate set Id=%s State=%d"),
			*ExperienceId.ToString(), (int32)LoadState); // [ADD]
		return;
	}

	// [ADD] DoD: Load 시작 로그(서버 확정 순간)
	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][SV] Load %s"), *ExperienceId.ToString());

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

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;
	const TCHAR* NetTag =
		(NM == NM_DedicatedServer || NM == NM_ListenServer) ? TEXT("SV") :
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST"); // [ADD]

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][%s] OnRep_CurrentExperienceId Id=%s State=%d"),
		NetTag, *CurrentExperienceId.ToString(), (int32)LoadState); // [ADD]

	// 핵심: 전환 지원
	if (LoadState != EMosesExperienceLoadState::Unloaded)
	{
		UE_LOG(LogMosesExp, Verbose, TEXT("[EXP][%s] ResetForNewExperience (PrevState=%d)"),
			NetTag, (int32)LoadState); // [ADD]
		ResetForNewExperience();
	}

	PendingExperienceId = CurrentExperienceId;
	StartLoadExperienceAssets();
}

void UMosesExperienceManagerComponent::StartLoadExperienceAssets()
{
	LoadState = EMosesExperienceLoadState::LoadingAssets;

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;
	const TCHAR* NetTag =
		(NM == NM_DedicatedServer || NM == NM_ListenServer) ? TEXT("SV") :
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST"); // [ADD]

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][%s] LoadingAssets -> %s"),
		NetTag, *CurrentExperienceId.ToString()); // [ADD]

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
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP] AssetsLoaded STALE Pending=%s Current=%s -> IGNORE"),
			*PendingExperienceId.ToString(), *CurrentExperienceId.ToString()); // [ADD]
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

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP] AssetsLoaded OK Id=%s -> StartLoadGameFeatures"),
		*CurrentExperienceId.ToString()); // [ADD]

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

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP] LoadingGameFeatures Id=%s Count=%d"),
		*CurrentExperienceId.ToString(), Features.Num()); // [ADD]

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
			// [ADD] 실패 사유를 반드시 남겨라(지금까지 silent였던 핵심 원인)
			bAnyGFFailed = true;
			LastGFFailReason = FString::Printf(TEXT("MakeGameFeaturePluginURL failed. PluginName=%s"), *PluginName);
			UE_LOG(LogMosesExp, Error, TEXT("[EXP][GF] URL FAIL Plugin=%s"), *PluginName); // [ADD]

			CompletedGFCount++;
			continue;
		}

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][GF] Activate REQ Plugin=%s URL=%s"),
			*PluginName, *URL); // [ADD]

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

		UE_LOG(LogMosesExp, Error, TEXT("[EXP][GF] Activate FAIL Plugin=%s Error=%s (%d/%d)"),
			*PluginName, *LastGFFailReason, CompletedGFCount, PendingGFCount); // [ADD]
	}
	else
	{
		const FString URL = MakeGameFeaturePluginURL(PluginName);
		if (!URL.IsEmpty())
		{
			ActivatedGFURLs.AddUnique(URL);
		}

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][GF] Activate OK Plugin=%s (%d/%d)"),
			*PluginName, CompletedGFCount, PendingGFCount); // [ADD]
	}

	if (CompletedGFCount >= PendingGFCount)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][GF] AllCompleted AnyFail=%d LastFail=%s"),
			bAnyGFFailed ? 1 : 0,
			LastGFFailReason.IsEmpty() ? TEXT("None") : *LastGFFailReason); // [ADD]

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

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;

	// [ADD] Day1 DoD: Loaded 로그(서버에서 반드시 보여야 함)
	if (NM == NM_DedicatedServer || NM == NM_ListenServer)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][SV] Loaded %s"), *CurrentExperienceId.ToString());
	}
	else if (NM == NM_Client)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][CL] Loaded %s"), *CurrentExperienceId.ToString());
	}
	else
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][ST] Loaded %s"), *CurrentExperienceId.ToString());
	}

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
	LoadState = EMosesExperienceLoadState::Failed;

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;
	const TCHAR* NetTag =
		(NM == NM_DedicatedServer || NM == NM_ListenServer) ? TEXT("SV") :
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST"); // [ADD]

	UE_LOG(LogMosesExp, Error, TEXT("[EXP][%s][FAIL] Id=%s Reason=%s"),
		NetTag, *CurrentExperienceId.ToString(), *Reason); // [ADD]

	// 실패 시에도 이전 GF가 남아있으면 위험하므로 정리
	DeactivatePreviouslyActivatedGameFeatures();
}

void UMosesExperienceManagerComponent::ResetForNewExperience()
{
	// Experience 전환 핵심: 이전 GF 내리기
	DeactivatePreviouslyActivatedGameFeatures();

	UE_LOG(LogMosesExp, Verbose, TEXT("[EXP] ResetForNewExperience PrevId=%s PrevState=%d"),
		*CurrentExperienceId.ToString(), (int32)LoadState); // [ADD]

	LoadState = EMosesExperienceLoadState::Unloaded;
	CurrentExperience = nullptr;
	PendingExperienceId = FPrimaryAssetId();

	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;
	LastGFFailReason.Reset();

	// [MOD] 전환 중에도 외부가 등록한 대기 콜백이 꼬이지 않도록 초기화
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
