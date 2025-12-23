// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesExperienceManagerComponent.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"

#include "MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/System/MosesAssetManager.h"

UMosesExperienceManagerComponent::UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)   // ✅ 이게 핵심 (부모 생성자 호출)
{
	SetIsReplicatedByDefault(true);
}

void UMosesExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMosesExperienceManagerComponent, CurrentExperienceId);
}

void UMosesExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnMosesExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperienceDefinition);
	}
	else
	{
		OnExperienceLoaded.Add(MoveTemp(Delegate));
	}
}

void UMosesExperienceManagerComponent::ServerSetCurrentExperience(FPrimaryAssetId ExperienceId)
{
	//check(GetOwner());
	//check(GetOwner()->HasAuthority()); // ✅ 서버만 결정

	//// 이미 같은 값을 세팅했으면 스킵
	//if (CurrentExperienceId == ExperienceId && LoadState != EMosesExperienceLoadState::Unloaded)
	//{
	//	return;
	//}

	//CurrentExperienceId = ExperienceId;

	//UE_LOG(LogTemp, Log, TEXT("[YJ][Exp] SERVER chose ExperienceId = %s"), *CurrentExperienceId.ToString());

	//// ✅ 서버는 즉시 로딩 시작 (클라도 OnRep로 시작)
	//OnRep_CurrentExperienceId();
}

void UMosesExperienceManagerComponent::OnRep_CurrentExperienceId()
{
	if (!CurrentExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("Invalid CurrentExperienceId (replicated value is invalid)"));
		return;
	}

	// 이미 로딩 시작했다면 중복 방지
	if (LoadState != EMosesExperienceLoadState::Unloaded)
	{
		return;
	}

	//UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] Begin Load (NetMode=%d) Id=%s"),
	//	(int32)GetOwner()->GetNetMode(),
	//	*CurrentExperienceId.ToString());

	//CurrentExperienceDefinition = LoadExperienceDefinitionFromId(CurrentExperienceId);
	//if (!CurrentExperienceDefinition)
	//{
	//	FailExperienceLoad(FString::Printf(TEXT("Failed to load ExperienceDefinition from Id=%s"), *CurrentExperienceId.ToString()));
	//	return;
	//}

	//StartExperienceLoad();
}

const UMosesExperienceDefinition* UMosesExperienceManagerComponent::LoadExperienceDefinitionFromId(const FPrimaryAssetId& ExperienceId) const
{
	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Moses][Exp] GetPrimaryAssetPath failed: %s"), *ExperienceId.ToString());
		return nullptr;
	}

	UObject* LoadedObject = AssetPath.TryLoad();
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Error, TEXT("[Moses][Exp] TryLoad failed: %s (%s)"), *ExperienceId.ToString(), *AssetPath.ToString());
		return nullptr;
	}

	TSubclassOf<UMosesExperienceDefinition> ExperienceClass;

	// BP 기반이면 GeneratedClass가 실제 UClass
	if (UBlueprint* BP = Cast<UBlueprint>(LoadedObject))
	{
		ExperienceClass = Cast<UClass>(BP->GeneratedClass);
	}
	else
	{
		ExperienceClass = Cast<UClass>(LoadedObject);
	}

	if (!ExperienceClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[Moses][Exp] LoadedObject is not a valid Experience class: %s"), *LoadedObject->GetName());
		return nullptr;
	}

	return GetDefault<UMosesExperienceDefinition>(ExperienceClass);
}

void UMosesExperienceManagerComponent::StartExperienceLoad()
{
	UE_LOG(LogTemp, Log, TEXT("[ExperienceManager] StartExperienceLoad"));

	check(CurrentExperienceDefinition);
	check(LoadState == EMosesExperienceLoadState::Unloaded);

	LoadState = EMosesExperienceLoadState::Loading;

	// ✅ Experience 자신을 "번들 root"로 등록
	TSet<FPrimaryAssetId> BundleAssets;
	BundleAssets.Add(CurrentExperienceDefinition->GetPrimaryAssetId());

	// ✅ NetMode에 따라 로드할 번들을 결정
	// - DedicatedServer는 Client번들 불필요
	// - Client는 Server번들 불필요
	TArray<FName> BundlesToLoad;
	{
		const ENetMode NetMode = GetOwner()->GetNetMode();

		if (GIsEditor || NetMode != NM_DedicatedServer)
		{
			BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateClient);
		}
		if (GIsEditor || NetMode != NM_Client)
		{
			BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateServer);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] Bundle Load Start: %s | Bundles=%s"),
		*CurrentExperienceDefinition->GetPrimaryAssetId().ToString(),
		*FString::JoinBy(BundlesToLoad, TEXT(","), [](const FName& N) { return N.ToString(); }));

	const FStreamableDelegate OnLoaded =
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete);

	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	TSharedPtr<FStreamableHandle> Handle =
		AssetManager.ChangeBundleStateForPrimaryAssets(
			BundleAssets.Array(),
			BundlesToLoad,
			{},
			false,
			FStreamableDelegate(),
			FStreamableManager::AsyncLoadHighPriority);

	if (!Handle.IsValid())
	{
		// Handle이 아예 없을 때도 실패로 본다.
		FailExperienceLoad(TEXT("ChangeBundleStateForPrimaryAssets returned null handle"));
		return;
	}

	if (Handle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnLoaded);
	}
	else
	{
		Handle->BindCompleteDelegate(OnLoaded);
		Handle->BindCancelDelegate(OnLoaded);
	}
}

void UMosesExperienceManagerComponent::OnExperienceLoadComplete()
{
	check(LoadState == EMosesExperienceLoadState::Loading);

	GameFeaturePluginURLs.Reset();

	// ✅ ExperienceDefinition에 명시된 GameFeature Plugin을 URL로 변환
	for (const FString& PluginName : CurrentExperienceDefinition->GameFeaturesToEnable)
	{
		FString PluginURL;
		if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, PluginURL))
		{
			GameFeaturePluginURLs.AddUnique(PluginURL);
		}
		else
		{
			// 실무: 못 찾으면 경고는 띄우되 당장 실패 처리할지는 정책
			UE_LOG(LogTemp, Warning, TEXT("[Moses][Exp] GameFeature plugin name not found: %s"), *PluginName);
		}
	}

	NumGameFeaturePluginsLoading = GameFeaturePluginURLs.Num();

	UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] Bundle Loaded. GameFeatures=%d"), NumGameFeaturePluginsLoading);

	if (NumGameFeaturePluginsLoading > 0)
	{
		LoadState = EMosesExperienceLoadState::LoadingGameFeatures;

		for (const FString& PluginURL : GameFeaturePluginURLs)
		{
			UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
				PluginURL,
				FGameFeaturePluginLoadComplete::CreateLambda(
					[this, PluginURL](const auto& Result)
					{
						UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] GameFeature Activated: %s"), *PluginURL);

						if (--NumGameFeaturePluginsLoading <= 0)
						{
							OnExperienceFullLoadCompleted();
						}
					}
				)
			);
		}
	}
	else
	{
		OnExperienceFullLoadCompleted();
	}
}

void UMosesExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	UE_LOG(LogTemp, Log, TEXT("[ExperienceManager] Experience Loaded (READY)"));

	if (LoadState == EMosesExperienceLoadState::Failed)
	{
		return;
	}

	LoadState = EMosesExperienceLoadState::Loaded;

	// ✅ Ready 이벤트 중복 방지(실무 필수)
	if (!bNotifiedReadyOnce)
	{
		bNotifiedReadyOnce = true;

		UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] ✅ FULLY LOADED (READY): %s"),
			*CurrentExperienceDefinition->GetPrimaryAssetId().ToString());

		OnExperienceLoaded.Broadcast(CurrentExperienceDefinition);
		OnExperienceLoaded.Clear();
	}

	DebugDump();
}

void UMosesExperienceManagerComponent::FailExperienceLoad(const FString& Reason)
{
	LoadState = EMosesExperienceLoadState::Failed;

	UE_LOG(LogTemp, Error, TEXT("[Moses][Exp] ❌ LOAD FAILED: %s"), *Reason);
	DebugDump();

	// 실무 정책:
	// - 여기서 크래시를 내면 원인 찾기 쉬움(개발 단계)
	// - 하지만 포트폴리오는 "안전하게 실패"도 보여줄 가치가 있음
	// checkNoEntry(); // 개발 중이면 켜도 됨
}

const UMosesExperienceDefinition* UMosesExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(IsExperienceLoaded());
	return CurrentExperienceDefinition;
}

void UMosesExperienceManagerComponent::DebugDump() const
{
	UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] Dump ---------------------------"));
	UE_LOG(LogTemp, Log, TEXT("  State: %d (0=Unloaded,1=Loading,2=LoadingGF,3=Loaded,4=Failed)"), (int32)LoadState);
	UE_LOG(LogTemp, Log, TEXT("  CurrentExperienceId: %s"), *CurrentExperienceId.ToString());
	UE_LOG(LogTemp, Log, TEXT("  CurrentDefinition: %s"), CurrentExperienceDefinition ? *CurrentExperienceDefinition->GetName() : TEXT("null"));
	UE_LOG(LogTemp, Log, TEXT("----------------------------------------"));
}

