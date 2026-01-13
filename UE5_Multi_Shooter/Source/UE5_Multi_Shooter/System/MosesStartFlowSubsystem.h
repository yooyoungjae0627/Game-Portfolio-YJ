#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesStartFlowSubsystem.generated.h"

class UMosesExperienceDefinition;
class UMosesExperienceManagerComponent;
class UMosesUserSessionSubsystem;
class UUserWidget;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesStartFlowSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Start UI에서 Enter 치면 이 함수 호출하게 연결
	UFUNCTION(BlueprintCallable, Category="Moses|StartFlow")
	void SubmitNicknameAndEnterLobby(const FString& Nickname);

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void TryBindExperienceReady();
	void HandleExperienceReady(const UMosesExperienceDefinition* Experience);
	void ShowStartUIFromExperience(const UMosesExperienceDefinition* Experience);

	UMosesExperienceManagerComponent* FindExperienceManager() const;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartWidget;

	FTimerHandle BindRetryTimerHandle;
};
