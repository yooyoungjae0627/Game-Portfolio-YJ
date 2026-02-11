// ============================================================================
// MosesExperienceManagerComponent.h (CLEAN)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "GameFeaturesSubsystem.h"

#include "MosesExperienceManagerComponent.generated.h"

class UMosesExperienceDefinition;
class UAbilitySystemComponent;

UENUM()
enum class EMosesExperienceLoadState : uint8
{
	Unloaded,
	LoadingAssets,
	LoadingGameFeatures,
	Loaded,
	Failed
};

DECLARE_DELEGATE_OneParam(FMosesExperienceLoadedDelegate, const UMosesExperienceDefinition* /*Experience*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesExperienceLoaded, const UMosesExperienceDefinition* /*Experience*/);

/**
 * UMosesExperienceManagerComponent
 *
 * [역할]
 * - 서버가 확정한 ExperienceId를 서버/클라 모두에서 로딩한다.
 * - ExperienceDefinition 로딩 → GameFeature Load/Activate → READY 브로드캐스트
 *
 * [Experience 전환 지원]
 * - Warmup → Combat → Result 처럼 ExperienceId가 바뀌면
 *   이전 GF를 Deactivate/Unload 해서 조립형 구조를 완성한다.
 *
 * [중요]
 * - Tick 금지(이 컴포넌트는 이벤트/콜백 기반)
 * - Experience 결정은 서버 RPC(권위)로만 수행
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// -------------------------------------------------------------------------
	// Replication
	// -------------------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------------------------------------------------------------------------
	// Server API
	// -------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	// -------------------------------------------------------------------------
	// Ready callback
	// -------------------------------------------------------------------------
	void CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate Delegate);

	bool IsExperienceLoaded() const { return LoadState == EMosesExperienceLoadState::Loaded; }
	bool HasLoadFailed() const { return LoadState == EMosesExperienceLoadState::Failed; }

	const UMosesExperienceDefinition* GetCurrentExperienceChecked() const;
	const UMosesExperienceDefinition* GetCurrentExperienceOrNull() const;

	/** Experience READY 이벤트(외부가 구독) */
	FOnMosesExperienceLoaded OnExperienceLoaded;

private:
	// -------------------------------------------------------------------------
	// RepNotify
	// -------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_CurrentExperienceId();

private:
	// -------------------------------------------------------------------------
	// Load Steps
	// -------------------------------------------------------------------------
	void StartLoadExperienceAssets();
	void OnExperienceAssetsLoaded();

	void StartLoadGameFeatures();
	void OnOneGameFeatureActivated(const UE::GameFeatures::FResult& Result, FString PluginName);

	void FinishExperienceLoad();
	void FailExperienceLoad(const FString& Reason);

	// -------------------------------------------------------------------------
	// Experience Switch
	// -------------------------------------------------------------------------
	void ResetForNewExperience();
	void DeactivatePreviouslyActivatedGameFeatures();

	static FString MakeGameFeaturePluginURL(const FString& PluginName);

private:
	// -------------------------------------------------------------------------
	// Server-side payload apply (GAS)
	// -------------------------------------------------------------------------
	void ApplyServerSideExperiencePayload();
	void ApplyServerSideAbilitySet(const FSoftObjectPath& AbilitySetPath);

private:
	// -------------------------------------------------------------------------
	// Debug Helper (DEV)
	// -------------------------------------------------------------------------
	UMosesExperienceDefinition* ResolveAndLoadExperienceDefinition_Debug(const FPrimaryAssetId& ExperienceId) const;

private:
	// -------------------------------------------------------------------------
	// Replicated
	// -------------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperienceId)
	FPrimaryAssetId CurrentExperienceId;

private:
	// -------------------------------------------------------------------------
	// Runtime
	// -------------------------------------------------------------------------
	UPROPERTY(Transient)
	TObjectPtr<const UMosesExperienceDefinition> CurrentExperience = nullptr;

	UPROPERTY(Transient)
	FPrimaryAssetId PendingExperienceId;

	UPROPERTY(Transient)
	EMosesExperienceLoadState LoadState = EMosesExperienceLoadState::Unloaded;

private:
	// -------------------------------------------------------------------------
	// GameFeature tracking
	// -------------------------------------------------------------------------
	UPROPERTY(Transient)
	int32 PendingGFCount = 0;

	UPROPERTY(Transient)
	int32 CompletedGFCount = 0;

	UPROPERTY(Transient)
	bool bAnyGFFailed = false;

	UPROPERTY(Transient)
	FString LastGFFailReason;

	UPROPERTY(Transient)
	TArray<FString> ActivatedGFURLs;

private:
	// -------------------------------------------------------------------------
	// Callback storage
	// - UHT/GC 안정성을 위해 UPROPERTY로 들고 가지 않음(Delegate는 UObject 참조 X 가정)
	// -------------------------------------------------------------------------
	TArray<FMosesExperienceLoadedDelegate> PendingReadyCallbacks;
};
