// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFeatureAction.h"
#include "MosesGameFeatureAction_Log.generated.h"

/**
 * - GF가 Activate/Deactivate 될 때만 로그가 찍히는 "흔적" 액션
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesGameFeatureAction_Log : public UGameFeatureAction
{
	GENERATED_BODY()
	
public:
	// GF별로 메시지만 다르게 주입해서 Lobby/Match 분리 증명
	UPROPERTY(EditAnywhere, Category = "Moses|GF")
	FString ActivateMessage = TEXT("[GF] Activated");

	UPROPERTY(EditAnywhere, Category = "Moses|GF")
	FString DeactivateMessage = TEXT("[GF] Deactivated");

protected:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	
	
};
