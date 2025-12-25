// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesGameFeatureAction_Log.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

void UMosesGameFeatureAction_Log::OnGameFeatureActivating(FGameFeatureActivatingContext& /*Context*/)
{
	UE_LOG(LogMosesExp, Warning, TEXT("%s"), *ActivateMessage);
}

void UMosesGameFeatureAction_Log::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& /*Context*/)
{
	UE_LOG(LogMosesExp, Warning, TEXT("%s"), *DeactivateMessage);
}

