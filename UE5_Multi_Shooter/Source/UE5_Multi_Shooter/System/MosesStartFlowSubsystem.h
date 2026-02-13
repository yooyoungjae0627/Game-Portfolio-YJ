// ============================================================================
// UE5_Multi_Shooter/System/MosesStartFlowSubsystem.h  (FULL - REORDERED)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MosesStartFlowSubsystem.generated.h"

class UUserWidget;
class UMosesExperienceDefinition;
class UMosesExperienceManagerComponent;

/**
 * UMosesStartFlowSubsystem
 *
 * [역할]
 * - Start 레벨에서만 "Start UI"를 띄운다.
 * - Experience READY 이벤트 기반 (Tick 금지)
 *
 * [흐름]
 * - PostLoadMapWithWorld로 맵 로드 감지
 * - GameState에서 ExperienceManager를 찾아 READY 콜백 등록
 * - Experience에서 StartWidgetClass를 읽어 UI 생성
 *
 * [주의]
 * - GameInstanceSubsystem은 서버/클라 모두 존재
 * - UI는 로컬 플레이어(클라)에서만 생성 의미가 있으므로 Local PC가 있을 때만 생성
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesStartFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --------------------------------------------------------------------
	// Engine
	// --------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// --------------------------------------------------------------------
	// Map lifecycle hooks
	// --------------------------------------------------------------------
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// --------------------------------------------------------------------
	// Experience binding
	// --------------------------------------------------------------------
	void TryBindExperienceReady();
	void HandleExperienceReady(const UMosesExperienceDefinition* Experience);

	// --------------------------------------------------------------------
	// UI
	// --------------------------------------------------------------------
	void ShowStartUIFromExperience(const UMosesExperienceDefinition* Experience);

	// --------------------------------------------------------------------
	// Helpers
	// --------------------------------------------------------------------
	UMosesExperienceManagerComponent* FindExperienceManager() const;
	APlayerController* GetLocalPlayerController() const;

private:
	// --------------------------------------------------------------------
	// State (UPROPERTY first)
	// --------------------------------------------------------------------
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartWidget = nullptr;

	// --------------------------------------------------------------------
	// State (non-UPROPERTY)
	// --------------------------------------------------------------------
	FTimerHandle BindRetryTimerHandle;
};
