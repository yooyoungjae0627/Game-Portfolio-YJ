#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesExperienceApplicatorSubsystem.generated.h"

class UInputMappingContext;
class UUserWidget;
class UMosesExperienceDefinition;
class UMosesExperienceManagerComponent;

/**
 * UMosesExperienceApplicatorSubsystem
 *
 * [역할]
 * - (클라) Experience READY를 감지하고,
 *   - HUD 위젯 생성/AddToViewport
 *   - EnhancedInput IMC 적용
 *
 * [중요 원칙]
 * - GF Action은 “생성/적용”을 하지 않는다. (중복 방지)
 * - Applicator는 “클라 적용(UI/IMC)”의 단일 책임자.
 *
 * [DAY2 안정성 규칙]
 * - ServerTravel/SeamlessTravel로 월드가 바뀌면 ExpManager를 다시 찾아 바인딩한다.
 * - Payload 적용은 "Clear -> Apply" 순서로만 한다.
 * - IMC는 Add 했으면 반드시 Remove도 가능해야 하므로 AppliedIMC 포인터를 유지한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesExperienceApplicatorSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// ----------------------------
	// ULocalPlayerSubsystem
	// ----------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// ----------------------------
	// Apply API
	// ----------------------------
	void ApplyCurrentExperiencePayload();
	void ClearApplied();

private:
	// ----------------------------
	// Experience binding (READY hook)
	// ----------------------------
	void StartRetryBindExperienceManager();
	void StopRetryBindExperienceManager();
	void TryBindExperienceManagerOnce();
	void HandleExperienceLoaded(const UMosesExperienceDefinition* Experience);

private:
	// ----------------------------
	// Internal helpers
	// ----------------------------
	void ApplyHUD();
	void ApplyInputMapping();
	void HandlePostLoadMap(UWorld* LoadedWorld);

private:
	// ----------------------------
	// Applied runtime instances
	// ----------------------------
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SpawnedHUDWidget = nullptr;

	// RemoveMappingContext에 쓰기 위해 non-const로 유지한다.
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedIMC = nullptr;

	UPROPERTY(Transient)
	int32 AppliedInputPriority = 0;

private:
	// ----------------------------
	// Runtime binding state
	// ----------------------------
	UPROPERTY(Transient)
	TWeakObjectPtr<UMosesExperienceManagerComponent> CachedExpManager;

	UPROPERTY(Transient)
	bool bBoundToExpManager = false;

	UPROPERTY(Transient)
	int32 RetryCount = 0;

	FTimerHandle RetryBindTimerHandle;
	FDelegateHandle PostLoadMapHandle;
};
