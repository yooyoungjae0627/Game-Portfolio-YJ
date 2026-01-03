#pragma once

#include "GameFeatureAction.h"
#include "UObject/SoftObjectPath.h"
#include "GameFeatureAction_MosesLobbyUI.generated.h"

/**
 * GameFeatureAction_MosesLobbyUI
 *
 * 역할:
 * - GameFeature 활성화 시
 *   "Lobby UI Widget 경로"를 전역 레지스트리에 등록
 *
 * 원칙:
 * - ❌ Subsystem / Player / Widget 직접 접근 금지
 * - ❌ CreateWidget 절대 금지
 * - ✅ 경로만 제공
 */
UCLASS(MinimalAPI)
class UGameFeatureAction_MosesLobbyUI : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "LobbyUI")
	FSoftClassPath LobbyWidgetClassPath =
		FSoftClassPath(TEXT("/GF_Lobby_Code/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C"));

};
