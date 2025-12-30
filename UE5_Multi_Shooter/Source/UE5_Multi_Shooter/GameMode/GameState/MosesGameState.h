// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"  
#include "MosesGameState.generated.h"

class UMosesExperienceManagerComponent;

UENUM(BlueprintType)
enum class EMosesServerPhase : uint8
{
	None  UMETA(DisplayName = "None"),
	Lobby UMETA(DisplayName = "Lobby"),
	Match UMETA(DisplayName = "Match"),
};

/**
 * 
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameState : public AGameStateBase
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

public:
	AMosesGameState(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버만 호출: Phase 확정(단일 소스) */
	void ServerSetPhase(EMosesServerPhase NewPhase);

	EMosesServerPhase GetCurrentPhase() const { return CurrentPhase; }

	/** Experience 로딩/Ready를 담당 */
	UPROPERTY()
	TObjectPtr<UMosesExperienceManagerComponent> ExperienceManagerComponent;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	EMosesServerPhase CurrentPhase = EMosesServerPhase::None;

	UFUNCTION()
	void OnRep_CurrentPhase();

};
