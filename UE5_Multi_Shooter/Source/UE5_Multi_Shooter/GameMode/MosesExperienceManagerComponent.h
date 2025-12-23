// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/GameStateComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "MosesExperienceManagerComponent.generated.h"

class UMosesExperienceDefinition;

/** Experience 로딩 상태 */
UENUM()
enum class EMosesExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	Loaded,
	Failed
};

/** 로딩 완료(Ready) 시 호출되는 델리게이트 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesExperienceLoaded, const UMosesExperienceDefinition* /*Experience*/);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()
	
public:
	UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** ✅ Ready 이벤트: 이미 Loaded면 즉시 실행, 아니면 완료 때 실행 */
	void CallOrRegister_OnExperienceLoaded(FOnMosesExperienceLoaded::FDelegate&& Delegate);

	/** ✅ 서버 전용: 이번 판 Experience 결정 */
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	/** 로딩 완료? */
	bool IsExperienceLoaded() const { return LoadState == EMosesExperienceLoadState::Loaded; }

	/** 로딩 실패? */
	bool HasLoadFailed() const { return LoadState == EMosesExperienceLoadState::Failed; }

	/** Loaded 이후 안전 접근 */
	const UMosesExperienceDefinition* GetCurrentExperienceChecked() const;

	/** 외부 구독 이벤트 */
	FOnMosesExperienceLoaded OnExperienceLoaded;

private:
	/** ✅ 서버가 결정한 ExperienceId를 클라에게 복제 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperienceId)
	FPrimaryAssetId CurrentExperienceId;

	/** 클라에서 ExperienceId 받으면 로드 시작 */
	UFUNCTION()
	void OnRep_CurrentExperienceId();

private:
	/** 로딩된 ExperienceDefinition(CDO) */
	UPROPERTY(Transient)
	TObjectPtr<const UMosesExperienceDefinition> CurrentExperienceDefinition = nullptr;

	/** 상태 */
	EMosesExperienceLoadState LoadState = EMosesExperienceLoadState::Unloaded;

	/** ✅ Ready 이벤트가 중복으로 나가지 않게 1회 가드 */
	bool bNotifiedReadyOnce = false;

	/** GameFeature plugin urls */
	TArray<FString> GameFeaturePluginURLs;

	/** 로딩 중 plugin 개수 */
	int32 NumGameFeaturePluginsLoading = 0;

private:
	/** ExperienceDefinition 로딩 (Id → CDO) */
	const UMosesExperienceDefinition* LoadExperienceDefinitionFromId(const FPrimaryAssetId& ExperienceId) const;

	/** Experience 번들 로딩 시작 */
	void StartExperienceLoad();

	/** 번들 로딩 완료 */
	void OnExperienceLoadComplete();

	/** 최종 완료 */
	void OnExperienceFullLoadCompleted();

	/** 실패 처리(크래시 대신 로그 + 상태 Failed) */
	void FailExperienceLoad(const FString& Reason);

private:
	/** ✅ (선택) 디버그: 현재 상태 덤프 */
	void DebugDump() const;
	
	
};
