#pragma once

#include "Components/GameStateComponent.h"
#include "UObject/PrimaryAssetId.h"

// UE::GameFeatures::FResult 타입을 콜백에서 쓰므로 헤더에서 포함
#include "GameFeaturesSubsystem.h"

#include "MosesExperienceManagerComponent.generated.h"

class UMosesExperienceDefinition;

/** Experience 로딩 단계 */
UENUM()
enum class EMosesExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	Loaded,
	Failed
};

/** Loaded(READY) 순간 1회 브로드캐스트 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesExperienceLoaded, const UMosesExperienceDefinition* /*Experience*/);

/** CallOrRegister용 1회성 콜백 */
using FMosesExperienceLoadedDelegate = TDelegate<void(const UMosesExperienceDefinition* /*Experience*/)>;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버가 Experience 결정(Authority) */
	UFUNCTION(Server, Reliable)
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	/** READY 보장 패턴: Loaded면 즉시, 아니면 Loaded 순간 1회 */
	void CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate&& Delegate);

	bool IsExperienceLoaded() const { return LoadState == EMosesExperienceLoadState::Loaded; }
	bool HasLoadFailed() const { return LoadState == EMosesExperienceLoadState::Failed; }

	const UMosesExperienceDefinition* GetCurrentExperienceChecked() const;

	FOnMosesExperienceLoaded OnExperienceLoaded;

protected:
	/** ExperienceId 복제되면(서버/클라) 여기서 로딩 시작 */
	UFUNCTION()
	void OnRep_CurrentExperienceId();

	/** ExperienceDefinition 로딩 완료 */
	void OnExperienceAssetsLoaded();

	/** Experience가 요구하는 GameFeature들을 Load+Activate */
	void StartLoadGameFeatures();

	/**
	 * UE5 GameFeatures 콜백 시그니처(중요)
	 * - LoadAndActivateGameFeaturePlugin의 Complete 델리게이트는
	 *   (const UE::GameFeatures::FResult&) 를 받는다.
	 * - PluginName은 CreateUObject로 캡처해 뒤에 인자로 받는다.
	 */
	void OnOneGameFeatureActivated(const UE::GameFeatures::FResult& Result, FString PluginName);

	/** PluginName -> file:/.../Plugin.uplugin */
	static FString MakeGameFeaturePluginURL(const FString& PluginName);

	/** 최종 READY 처리 */
	void OnExperienceFullLoadCompleted();

	void FailExperienceLoad(const FString& Reason);
	void ResetExperienceLoadState();
	void DebugDump() const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperienceId)
	FPrimaryAssetId CurrentExperienceId;

	UPROPERTY(Transient)
	TObjectPtr<const UMosesExperienceDefinition> CurrentExperience = nullptr;

	UPROPERTY(Transient)
	FString LastGFFailReason;

	EMosesExperienceLoadState LoadState = EMosesExperienceLoadState::Unloaded;
	bool bNotifiedReadyOnce = false;

	/** stale callback 방지 */
	FPrimaryAssetId PendingExperienceId;

	/** CallOrRegister용 1회성 콜백들 */
	TArray<FMosesExperienceLoadedDelegate> OnExperienceLoadedCallbacks;

	/** GF 로딩 추적 */
	int32 PendingGFCount = 0;
	int32 CompletedGFCount = 0;
	bool bAnyGFFailed = false;
};
