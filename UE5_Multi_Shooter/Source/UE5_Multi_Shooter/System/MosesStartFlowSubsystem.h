#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "MosesStartFlowSubsystem.generated.h"

class UUserWidget;
class UMosesExperienceDefinition;
class UMosesExperienceManagerComponent;

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

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartWidget = nullptr;

	FTimerHandle BindRetryTimerHandle;
};
