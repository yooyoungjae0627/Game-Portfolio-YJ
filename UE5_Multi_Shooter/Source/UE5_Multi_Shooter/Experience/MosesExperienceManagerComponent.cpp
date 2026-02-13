// ============================================================================
// UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.cpp  (FULL - REORDERED)
// ============================================================================

#include "UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesAbilitySet.h"
#include "UE5_Multi_Shooter/System/MosesAssetManager.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

#include "GameFeaturePluginOperationResult.h"

#include "Net/UnrealNetwork.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Engine/AssetManager.h"

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

// ============================================================================
// Server API
// ============================================================================

void UMosesExperienceManagerComponent::ServerSetCurrentExperience_Implementation(FPrimaryAssetId ExperienceId)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Experience", TEXT("Client attempted SetCurrentExperience"));

	check(GetOwner());
	check(GetOwner()->HasAuthority());

	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));

	if (!ExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("ServerSetCurrentExperience: Invalid ExperienceId"));
		return;
	}

	// 실수 방지: Type 강제 교정
	if (ExperienceId.PrimaryAssetType != ExperienceType)
	{
		ExperienceId = FPrimaryAssetId(ExperienceType, ExperienceId.PrimaryAssetName);
	}

	// 동일 Experience 중복 실행 방지
	if (CurrentExperienceId == ExperienceId && LoadState != EMosesExperienceLoadState::Unloaded)
	{
		UE_LOG(LogMosesExp, Verbose, TEXT("[EXP][SV] Ignore duplicate set Id=%s State=%d"),
			*ExperienceId.ToString(), (int32)LoadState);
		return;
	}

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][SV] Load %s"), *ExperienceId.ToString());

	// Experience 교체 시작(이전 GF 내리기 포함)
	ResetForNewExperience();

	CurrentExperienceId = ExperienceId;

	// 서버도 클라와 동일 루트 로딩을 타기 위해 RepNotify를 직접 호출
	OnRep_CurrentExperienceId();
}

// ============================================================================
// Ready Callback
// ============================================================================

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

const UMosesExperienceDefinition* UMosesExperienceManagerComponent::GetCurrentExperienceOrNull() const
{
	return IsExperienceLoaded() ? CurrentExperience : nullptr;
}

// ============================================================================
// RepNotify
// ============================================================================

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
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST");

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][%s] OnRep_CurrentExperienceId Id=%s State=%d"),
		NetTag, *CurrentExperienceId.ToString(), (int32)LoadState);

	// 전환 지원: 이미 뭔가 로딩 중이거나 Loaded였다면 리셋부터
	if (LoadState != EMosesExperienceLoadState::Unloaded)
	{
		UE_LOG(LogMosesExp, Verbose, TEXT("[EXP][%s] ResetForNewExperience (PrevState=%d)"),
			NetTag, (int32)LoadState);
		ResetForNewExperience();
	}

	PendingExperienceId = CurrentExperienceId;

#if !UE_BUILD_SHIPPING
	ResolveAndLoadExperienceDefinition_Debug(PendingExperienceId);
#endif

	StartLoadExperienceAssets();
}

// ============================================================================
// Load Steps: Assets
// ============================================================================

void UMosesExperienceManagerComponent::StartLoadExperienceAssets()
{
	LoadState = EMosesExperienceLoadState::LoadingAssets;

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;
	const TCHAR* NetTag =
		(NM == NM_DedicatedServer || NM == NM_ListenServer) ? TEXT("SV") :
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST");

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][%s] LoadingAssets -> %s"),
		NetTag, *CurrentExperienceId.ToString());

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
			*PendingExperienceId.ToString(), *CurrentExperienceId.ToString());
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
		*CurrentExperienceId.ToString());

	StartLoadGameFeatures();
}

// ============================================================================
// Load Steps: GameFeatures
// ============================================================================

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

void UMosesExperienceManagerComponent::StartLoadGameFeatures()
{
	check(CurrentExperience);

	LoadState = EMosesExperienceLoadState::LoadingGameFeatures;
	ActivatedGFURLs.Reset();

	const TArray<FString>& Features = CurrentExperience->GetGameFeaturesToEnable();

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP] LoadingGameFeatures Id=%s Count=%d"),
		*CurrentExperienceId.ToString(), Features.Num());

	if (Features.Num() == 0)
	{
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
			LastGFFailReason = FString::Printf(TEXT("MakeGameFeaturePluginURL failed. PluginName=%s"), *PluginName);

			UE_LOG(LogMosesExp, Error, TEXT("[EXP][GF] URL FAIL Plugin=%s"), *PluginName);

			CompletedGFCount++;
			continue;
		}

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][GF] Activate REQ Plugin=%s URL=%s"),
			*PluginName, *URL);

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
			*PluginName, *LastGFFailReason, CompletedGFCount, PendingGFCount);
	}
	else
	{
		const FString URL = MakeGameFeaturePluginURL(PluginName);
		if (!URL.IsEmpty())
		{
			ActivatedGFURLs.AddUnique(URL);
		}

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][GF] Activate OK Plugin=%s (%d/%d)"),
			*PluginName, CompletedGFCount, PendingGFCount);
	}

	if (CompletedGFCount >= PendingGFCount)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][GF] AllCompleted AnyFail=%d LastFail=%s"),
			bAnyGFFailed ? 1 : 0,
			LastGFFailReason.IsEmpty() ? TEXT("None") : *LastGFFailReason);

		if (bAnyGFFailed)
		{
			FailExperienceLoad(FString::Printf(TEXT("GameFeature activation failed. Last=%s"), *LastGFFailReason));
			return;
		}

		FinishExperienceLoad();
	}
}

// ============================================================================
// Finish / Fail
// ============================================================================

void UMosesExperienceManagerComponent::FinishExperienceLoad()
{
	LoadState = EMosesExperienceLoadState::Loaded;

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;

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

	OnExperienceLoaded.Broadcast(CurrentExperience);

	for (FMosesExperienceLoadedDelegate& CB : PendingReadyCallbacks)
	{
		CB.ExecuteIfBound(CurrentExperience);
	}
	PendingReadyCallbacks.Reset();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ApplyServerSideExperiencePayload();
	}
}

void UMosesExperienceManagerComponent::FailExperienceLoad(const FString& Reason)
{
	LoadState = EMosesExperienceLoadState::Failed;

	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;
	const TCHAR* NetTag =
		(NM == NM_DedicatedServer || NM == NM_ListenServer) ? TEXT("SV") :
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST");

	UE_LOG(LogMosesExp, Error, TEXT("[EXP][%s][FAIL] Id=%s Reason=%s"),
		NetTag, *CurrentExperienceId.ToString(), *Reason);

	DeactivatePreviouslyActivatedGameFeatures();
}

// ============================================================================
// Experience Switch
// ============================================================================

void UMosesExperienceManagerComponent::ResetForNewExperience()
{
	DeactivatePreviouslyActivatedGameFeatures();

	UE_LOG(LogMosesExp, Verbose, TEXT("[EXP] ResetForNewExperience PrevId=%s PrevState=%d"),
		*CurrentExperienceId.ToString(), (int32)LoadState);

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
		GFS.DeactivateGameFeaturePlugin(URL);
		GFS.UnloadGameFeaturePlugin(URL);
	}

	ActivatedGFURLs.Reset();
}

// ============================================================================
// Server-side payload apply (GAS)
// ============================================================================

void UMosesExperienceManagerComponent::ApplyServerSideExperiencePayload()
{
	check(GetOwner() && GetOwner()->HasAuthority());

	if (!CurrentExperience)
	{
		return;
	}

	const FSoftObjectPath AbilitySetPath = CurrentExperience->GetAbilitySetPath();
	if (AbilitySetPath.IsNull())
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][SV] AbilitySetPath is empty (Skip GAS Apply)"));
		return;
	}

	ApplyServerSideAbilitySet(AbilitySetPath);
}

void UMosesExperienceManagerComponent::ApplyServerSideAbilitySet(const FSoftObjectPath& AbilitySetPath)
{
	check(GetOwner() && GetOwner()->HasAuthority());

	UObject* LoadedObj = AbilitySetPath.TryLoad();
	const UMosesAbilitySet* AbilitySet = Cast<UMosesAbilitySet>(LoadedObj);
	if (!AbilitySet)
	{
		UE_LOG(LogMosesExp, Error, TEXT("[EXP][SV] AbilitySet Load FAIL Path=%s"), *AbilitySetPath.ToString());
		return;
	}

	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		UAbilitySystemComponent* ASC = nullptr;
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS))
		{
			ASC = ASI->GetAbilitySystemComponent();
		}

		if (!ASC)
		{
			continue;
		}

		FMosesAbilitySet_GrantedHandles Handles;
		AbilitySet->GiveToAbilitySystem(*ASC, Handles);

		UE_LOG(LogMosesGAS, Warning, TEXT("[EXP][SV][GAS] AbilitySet Applied PS=%s Set=%s"),
			*GetNameSafe(PS), *AbilitySetPath.ToString());
	}
}

// ============================================================================
// [DEV] Debug Helper
// ============================================================================

UMosesExperienceDefinition* UMosesExperienceManagerComponent::ResolveAndLoadExperienceDefinition_Debug(const FPrimaryAssetId& ExperienceId) const
{
	if (!ExperienceId.IsValid())
	{
		UE_LOG(LogMosesExp, Error, TEXT("[EXP][DBG] Invalid ExperienceId"));
		return nullptr;
	}

	UAssetManager& AM = UAssetManager::Get();

	const FSoftObjectPath ResolvedPath = AM.GetPrimaryAssetPath(ExperienceId);

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][DBG] Resolve Id=%s Type=%s Name=%s Path=%s"),
		*ExperienceId.ToString(),
		*ExperienceId.PrimaryAssetType.ToString(),
		*ExperienceId.PrimaryAssetName.ToString(),
		*ResolvedPath.ToString());

	if (!ResolvedPath.IsValid())
	{
		UE_LOG(LogMosesExp, Error,
			TEXT("[EXP][DBG][FAIL] PrimaryAssetPath EMPTY -> AssetManager Scan/Type mismatch likely. Id=%s"),
			*ExperienceId.ToString());
		return nullptr;
	}

	UObject* LoadedObj = ResolvedPath.TryLoad();
	UMosesExperienceDefinition* LoadedExp = Cast<UMosesExperienceDefinition>(LoadedObj);

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][DBG] LoadResult Id=%s Obj=%s Class=%s CastToExp=%s"),
		*ExperienceId.ToString(),
		*GetNameSafe(LoadedObj),
		LoadedObj ? *LoadedObj->GetClass()->GetName() : TEXT("None"),
		*GetNameSafe(LoadedExp));

	if (!LoadedExp)
	{
		UE_LOG(LogMosesExp, Error,
			TEXT("[EXP][DBG][FAIL] TryLoad returned null or wrong class. Id=%s Path=%s"),
			*ExperienceId.ToString(),
			*ResolvedPath.ToString());
	}

	return LoadedExp;
}
