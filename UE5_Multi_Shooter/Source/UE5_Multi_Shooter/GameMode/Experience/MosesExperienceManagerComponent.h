#pragma once

#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "Components/GameStateComponent.h"
#include "UObject/PrimaryAssetId.h"                 // [MOD] PrimaryAssetId 안정 경로
#include "Delegates/Delegate.h"                     // [ADD] DECLARE_DELEGATE_*
#include "Delegates/DelegateCombinations.h"         // [ADD] DECLARE_MULTICAST_DELEGATE_*
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
 * - ExperienceDefinition 로딩 -> GameFeature Load/Activate -> READY 브로드캐스트
 *
 * [Experience 전환 지원]
 * - Warmup -> Combat -> Result 처럼 ExperienceId가 바뀌면
 *   이전 GF들을 Deactivate/Unload 해서 “조립형” 구조를 완성한다.
 *
 * [중요]
 * - Tick 금지
 * - 서버 권위: Experience 결정은 서버 RPC로만 수행한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ----------------------------
	// UActorComponent
	// ----------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ----------------------------
	// Server API
	// ----------------------------
	UFUNCTION(Server, Reliable)
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	// ----------------------------
	// Ready callback
	// ----------------------------
	void CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate Delegate);

	bool IsExperienceLoaded() const { return LoadState == EMosesExperienceLoadState::Loaded; }
	bool HasLoadFailed() const { return LoadState == EMosesExperienceLoadState::Failed; }

	const UMosesExperienceDefinition* GetCurrentExperienceChecked() const;

	// ================================
	// [DEV] Experience Resolve/Load Debug Helper
	// - PrimaryAssetId("Experience:Exp_Match_Warmup")를 AssetManager로 해석하고
	// - SoftObjectPath를 TryLoad해서 실제 ExperienceDefinition 로드 성공/실패를 로그로 증명한다.
	// ================================
private:
	class UMosesExperienceDefinition* ResolveAndLoadExperienceDefinition_Debug(const FPrimaryAssetId& ExperienceId) const;

public:
	/** Experience READY 이벤트(외부가 구독) */
	FOnMosesExperienceLoaded OnExperienceLoaded;

public:
	/** 현재 로드 완료된 ExperienceDefinition을 반환. 아직 준비 전이면 nullptr. */
	const UMosesExperienceDefinition* GetCurrentExperienceOrNull() const;


private:
	// ----------------------------
	// Replication
	// ----------------------------
	UFUNCTION()
	void OnRep_CurrentExperienceId();

	// ----------------------------
	// Load Steps
	// ----------------------------
	void StartLoadExperienceAssets();
	void OnExperienceAssetsLoaded();

	void StartLoadGameFeatures();
	void OnOneGameFeatureActivated(const UE::GameFeatures::FResult& Result, FString PluginName);

	void FinishExperienceLoad();
	void FailExperienceLoad(const FString& Reason);

	// ----------------------------
	// Experience Switch
	// ----------------------------
	void ResetForNewExperience();
	void DeactivatePreviouslyActivatedGameFeatures();

	static FString MakeGameFeaturePluginURL(const FString& PluginName);

private:
	// ------------------------------------------------------------
	// Server-side payload apply (GAS)
	// ------------------------------------------------------------
	void ApplyServerSideExperiencePayload();
	void ApplyServerSideAbilitySet(const FSoftObjectPath& AbilitySetPath);

private:
	// ----------------------------
	// Replicated
	// ----------------------------
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperienceId)
	FPrimaryAssetId CurrentExperienceId;

	// ----------------------------
	// Runtime
	// ----------------------------
	UPROPERTY(Transient)
	TObjectPtr<const UMosesExperienceDefinition> CurrentExperience = nullptr;

	UPROPERTY(Transient)
	FPrimaryAssetId PendingExperienceId;

	UPROPERTY(Transient)
	EMosesExperienceLoadState LoadState = EMosesExperienceLoadState::Unloaded;

	// ----------------------------
	// GameFeature tracking
	// ----------------------------
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

	// ----------------------------
	// Callback storage (UHT 안정성을 위해 UPROPERTY 사용 안 함)
	// ----------------------------
	TArray<FMosesExperienceLoadedDelegate> PendingReadyCallbacks;
};
