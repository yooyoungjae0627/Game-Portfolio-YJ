// ============================================================================
// UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h  (FULL - REORDERED)
// ----------------------------------------------------------------------------
// [정리 원칙]
// - UPROPERTY: Replicated/Transient/Config/Defaults 순으로 "먼저" 배치
// - UPROPERTY 없는 런타임/가드/델리게이트 저장 변수는 마지막
// - 함수는 역할(Replication/Load/Switch/Payload) 단위로 묶고,
//   각 함수 설명을 헤더에 기입
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "UObject/SoftObjectPath.h"
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
 * - 서버가 확정한 ExperienceId(CurrentExperienceId)를 Replicate 한다.
 * - 서버/클라 모두 동일 루트로:
 *   (1) ExperienceDefinition 로드
 *   (2) GameFeature Load/Activate
 *   (3) READY 브로드캐스트
 *
 * [Experience 전환]
 * - Warmup -> Combat -> Result 처럼 ExperienceId가 바뀌면,
 *   이전 Experience가 활성화한 GameFeature를 Deactivate/Unload 한다.
 *
 * [정책]
 * - Tick 금지
 * - Experience 선택/변경은 서버 RPC(ServerSetCurrentExperience)로만 수행
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// --------------------------------------------------------------------
	// UActorComponent
	// --------------------------------------------------------------------
	/** CurrentExperienceId 복제를 위해 Replication 등록 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --------------------------------------------------------------------
	// Server API
	// --------------------------------------------------------------------
	/**
	 * 서버가 "이번 매치/페이즈 Experience"를 최종 확정한다.
	 * - CurrentExperienceId 세팅
	 * - ResetForNewExperience(이전 GF 내림)
	 * - OnRep_CurrentExperienceId를 서버에서도 직접 호출해 동일 로딩 루트를 탄다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	// --------------------------------------------------------------------
	// Ready Callback
	// --------------------------------------------------------------------
	/**
	 * Experience READY 시점에 호출할 델리게이트를 등록한다.
	 * - 이미 Loaded면 즉시 Execute
	 * - 아니면 PendingReadyCallbacks에 저장 후 FinishExperienceLoad에서 실행
	 */
	void CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate Delegate);

	/** 현재 Experience가 완전히 Loaded(READY) 상태인지 */
	bool IsExperienceLoaded() const { return LoadState == EMosesExperienceLoadState::Loaded; }

	/** 로딩 실패 상태인지 */
	bool HasLoadFailed() const { return LoadState == EMosesExperienceLoadState::Failed; }

	/**
	 * Loaded 상태에서만 CurrentExperience를 반환(체크 포함).
	 * - Loaded가 아니면 assert
	 */
	const UMosesExperienceDefinition* GetCurrentExperienceChecked() const;

	/**
	 * Loaded가 아니면 nullptr.
	 * - UI 등 "안전하게 조회"용
	 */
	const UMosesExperienceDefinition* GetCurrentExperienceOrNull() const;

public:
	/** Experience READY 이벤트(외부 구독용, Tick/Binding 없이 Delegate로만 갱신) */
	FOnMosesExperienceLoaded OnExperienceLoaded;

private:
	// --------------------------------------------------------------------
	// Replication
	// --------------------------------------------------------------------
	/**
	 * CurrentExperienceId가 Replicate 되었을 때(서버 포함) 로딩 파이프라인을 시작한다.
	 * - ResetForNewExperience(전환 지원)
	 * - PendingExperienceId 설정(스테일 방지)
	 * - StartLoadExperienceAssets()
	 */
	UFUNCTION()
	void OnRep_CurrentExperienceId();

	// --------------------------------------------------------------------
	// Load Steps
	// --------------------------------------------------------------------
	/** ExperienceDefinition(PrimaryAsset) 로드 시작 */
	void StartLoadExperienceAssets();

	/** PrimaryAsset 로드 완료 콜백 → GF 로드 단계로 전환 */
	void OnExperienceAssetsLoaded();

	/** Experience에 명시된 GameFeature들을 Load+Activate 시작 */
	void StartLoadGameFeatures();

	/** 개별 GF Activate 완료 콜백(성공/실패 누적) */
	void OnOneGameFeatureActivated(const UE::GameFeatures::FResult& Result, FString PluginName);

	/**
	 * 최종 READY 확정
	 * - LoadState=Loaded
	 * - OnExperienceLoaded.Broadcast
	 * - PendingReadyCallbacks 실행
	 * - (서버) ApplyServerSideExperiencePayload()
	 */
	void FinishExperienceLoad();

	/** 실패 처리(LoadState=Failed) + 이전 GF 정리 */
	void FailExperienceLoad(const FString& Reason);

	// --------------------------------------------------------------------
	// Experience Switch
	// --------------------------------------------------------------------
	/**
	 * Experience 전환을 위한 초기화
	 * - DeactivatePreviouslyActivatedGameFeatures()
	 * - 런타임 상태값/카운터/가드 리셋
	 */
	void ResetForNewExperience();

	/**
	 * 이전 Experience가 Activate했던 GF들을 Deactivate + Unload 한다.
	 * - "조립형 구조"의 핵심
	 */
	void DeactivatePreviouslyActivatedGameFeatures();

	/** PluginName -> "file:<uplugin full path>" URL 생성 */
	static FString MakeGameFeaturePluginURL(const FString& PluginName);

	// --------------------------------------------------------------------
	// Server-side payload apply (GAS)
	// --------------------------------------------------------------------
	/**
	 * 서버 권위로 Experience Payload를 적용한다.
	 * - 현재 Experience의 AbilitySetPath가 있으면 ApplyServerSideAbilitySet()
	 */
	void ApplyServerSideExperiencePayload();

	/** AbilitySet을 로드하여 GameState의 모든 PlayerState ASC에 GiveToAbilitySystem */
	void ApplyServerSideAbilitySet(const FSoftObjectPath& AbilitySetPath);

private:
	// =====================================================================
	// UPROPERTY 영역 (정렬: Replicated -> Transient Runtime -> etc)
	// =====================================================================

	// ----------------------------
	// Replicated
	// ----------------------------
	/** 서버가 확정한 ExperienceId(클라에 replicate) */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperienceId)
	FPrimaryAssetId CurrentExperienceId;

	// ----------------------------
	// Runtime (Transient)
	// ----------------------------
	/** 현재 로드 완료된 ExperienceDefinition(READY 이후 유효) */
	UPROPERTY(Transient)
	TObjectPtr<const UMosesExperienceDefinition> CurrentExperience = nullptr;

	/** 스테일 방지용: OnRep 시작 시 PendingExperienceId=CurrentExperienceId로 고정 */
	UPROPERTY(Transient)
	FPrimaryAssetId PendingExperienceId;

	/** 현재 로딩 상태 */
	UPROPERTY(Transient)
	EMosesExperienceLoadState LoadState = EMosesExperienceLoadState::Unloaded;

	// ----------------------------
	// GameFeature tracking (Transient)
	// ----------------------------
	UPROPERTY(Transient)
	int32 PendingGFCount = 0;

	UPROPERTY(Transient)
	int32 CompletedGFCount = 0;

	UPROPERTY(Transient)
	bool bAnyGFFailed = false;

	UPROPERTY(Transient)
	FString LastGFFailReason;

	/** 활성화한 GF URL 목록(전환 시 Deactivate/Unload 용) */
	UPROPERTY(Transient)
	TArray<FString> ActivatedGFURLs;

	// ----------------------------
	// Callback storage (UHT 안정: UPROPERTY 사용 안 함)
	// ----------------------------
	/** Loaded 전 등록된 READY 콜백들(FinishExperienceLoad에서 실행) */
	TArray<FMosesExperienceLoadedDelegate> PendingReadyCallbacks;

private:
	// --------------------------------------------------------------------
	// [DEV] Debug Helper
	// --------------------------------------------------------------------
	/**
	 * PrimaryAssetId를 AssetManager로 Resolve 후 TryLoad 해보고,
	 * "Path Empty" vs "Load null"을 로그로 분리한다. (Shipping 제외)
	 */
	class UMosesExperienceDefinition* ResolveAndLoadExperienceDefinition_Debug(const FPrimaryAssetId& ExperienceId) const;
};
