// ============================================================================
// MosesExperienceApplicatorSubsystem.h
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesExperienceApplicatorSubsystem.generated.h"

class UInputMappingContext;
class UUserWidget;
class UMosesExperienceDefinition;
class UMosesExperienceManagerComponent; 

/**
 * UMosesExperienceApplicatorSubsystem
 *
 * [한 줄 역할]
 * - 클라이언트 로컬에서 “Experience Payload(HUD/IMC)”를 실제로 적용한다.
 *
 * [중요]
 * - Tick/Binding 금지
 * - Experience READY(Loaded) 이벤트에 구독해서,
 *   READY 시점에 Registry Set → Apply 를 수행한다.
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

	// 월드 변경 감지용
	void HandlePostLoadMap(UWorld* LoadedWorld);

private:
	// ----------------------------
	// Applied runtime instances
	// ----------------------------
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SpawnedHUDWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UInputMappingContext> AppliedIMC = nullptr;

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
