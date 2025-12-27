#include "GameFeature/GameFeatureAction_MosesLobbyUI.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

void UGameFeatureAction_MosesLobbyUI::ForEachLocalPlayer_Do(UWorld* World, TFunctionRef<void(ULocalPlayer*)> Fn)
{
	if (!World) return;

	/**
	 * 공부 메모)
	 * - GameInstance는 로컬 플레이어 목록을 들고 있다.
	 * - 스플릿 스크린/로컬 멀티까지 고려하면 "LocalPlayers 배열 순회"가 정석이다.
	 */
	if (UGameInstance* GI = World->GetGameInstance())
	{
		for (ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (LP) Fn(LP);
		}
	}
}

void UGameFeatureAction_MosesLobbyUI::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF_LobbyUI] Activating")); // ✅ 여기 (함수 시작하자마자)

	/**
	 * - 엔진에는 WorldContext가 여러 개 있을 수 있다. (PIE, Editor Preview 등)
	 * - 그래서 GEngine->GetWorldContexts()를 순회하며 유효한 월드에 적용한다.
	 */
	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UWorld* World = WC.World();
		if (!World) continue;

		ForEachLocalPlayer_Do(World, [&](ULocalPlayer* LP)
			{
				if (UMosesLobbyLocalPlayerSubsystem* Sub = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
				{
					Sub->ActivateLobbyUI();
				}
			});
	}
}

void UGameFeatureAction_MosesLobbyUI::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[GF_LobbyUI] Deactivating"));

	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UWorld* World = WC.World();
		if (!World) continue;

		ForEachLocalPlayer_Do(World, [&](ULocalPlayer* LP)
			{
				if (UMosesLobbyLocalPlayerSubsystem* Sub = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
				{
					Sub->DeactivateLobbyUI();
				}
			});
	}
}
