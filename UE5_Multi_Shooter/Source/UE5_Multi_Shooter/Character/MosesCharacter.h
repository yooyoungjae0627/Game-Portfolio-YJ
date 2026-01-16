#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MosesCharacter.generated.h"

class UMosesPawnExtensionComponent;
class UMosesCameraComponent;
class UMosesHeroComponent;

class AMosesPlayerState;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMosesCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;

private:
	// GAS Init
	void TryInitASC_FromPlayerState();

	// LateJoin retry
	void StartLateJoinInitTimer();
	void StopLateJoinInitTimer();
	void LateJoinInitTick();

private:
	FTimerHandle TimerHandle_LateJoinInit;
	int32 LateJoinRetryCount = 0;

	static constexpr int32 MaxLateJoinRetries = 10;
	static constexpr float LateJoinRetryIntervalSec = 0.2f;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesPawnExtensionComponent> PawnExtComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent;
};
