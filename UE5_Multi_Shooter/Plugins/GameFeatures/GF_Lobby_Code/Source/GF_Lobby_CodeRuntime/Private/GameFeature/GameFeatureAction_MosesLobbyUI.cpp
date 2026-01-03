#include "GameFeature/GameFeatureAction_MosesLobbyUI.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

// 개발자 주석:
// - GF Action은 UI를 "생성"하지 않는다.
// - 오직 "경로 등록"만 한다.
// - 실제 CreateWidget/AddToViewport는 LocalPlayerSubsystem 책임.

void UGameFeatureAction_MosesLobbyUI::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("[GF_LobbyUI] Activating Path=%s"), *LobbyWidgetClassPath.ToString());

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GF_LobbyUI] Activating FAIL (No GEngine)"));
		return;
	}

	int32 RegisteredCount = 0;

	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UGameInstance* GI = WC.OwningGameInstance;
		if (!GI)
		{
			continue;
		}

		if (UMosesUIRegistrySubsystem* Registry = GI->GetSubsystem<UMosesUIRegistrySubsystem>())
		{
			Registry->SetLobbyWidgetClassPath(LobbyWidgetClassPath);
			++RegisteredCount;

			UE_LOG(LogTemp, Log, TEXT("[GF_LobbyUI] Registered GI=%s Path=%s"),
				*GetNameSafe(GI),
				*LobbyWidgetClassPath.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[GF_LobbyUI] Activating Done RegisteredCount=%d"), RegisteredCount);
}

void UGameFeatureAction_MosesLobbyUI::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("[GF_LobbyUI] Deactivating"));

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GF_LobbyUI] Deactivating FAIL (No GEngine)"));
		return;
	}

	int32 ClearedCount = 0;

	for (const FWorldContext& WC : GEngine->GetWorldContexts())
	{
		UGameInstance* GI = WC.OwningGameInstance;
		if (!GI)
		{
			continue;
		}

		if (UMosesUIRegistrySubsystem* Registry = GI->GetSubsystem<UMosesUIRegistrySubsystem>())
		{
			Registry->ClearLobbyWidgetClassPath();
			++ClearedCount;

			UE_LOG(LogTemp, Log, TEXT("[GF_LobbyUI] Cleared GI=%s"), *GetNameSafe(GI));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[GF_LobbyUI] Deactivating Done ClearedCount=%d"), ClearedCount);
}
