// UE5_Multi_Shooter/System/MosesStartFlowSubsystem.h
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
 * - Start 레벨에서만 "Start UI(로비/시작 위젯)"를 띄운다.
 * - Experience READY 이벤트를 기준으로 동작한다 (Tick 금지)
 *
 * [동작 흐름]
 * - PostLoadMapWithWorld 이벤트로 맵 로드 시점 감지
 * - GameState에서 ExperienceManagerComponent를 찾아 READY 콜백 등록
 * - Experience에 설정된 StartWidgetClass를 읽어 UI 생성
 *
 * [주의]
 * - 이 Subsystem은 GameInstance에 붙으므로 서버/클라 모두 존재한다.
 * - UI 생성은 로컬 플레이어(클라)에서만 의미가 있으므로
 *   PlayerController가 존재하는 시점에만 생성한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesStartFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void TryBindExperienceReady();
	void HandleExperienceReady(const UMosesExperienceDefinition* Experience);

	void ShowStartUIFromExperience(const UMosesExperienceDefinition* Experience);

	UMosesExperienceManagerComponent* FindExperienceManager() const;
	APlayerController* GetLocalPlayerController() const; // [ADD]

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartWidget = nullptr;

	FTimerHandle BindRetryTimerHandle;
};
