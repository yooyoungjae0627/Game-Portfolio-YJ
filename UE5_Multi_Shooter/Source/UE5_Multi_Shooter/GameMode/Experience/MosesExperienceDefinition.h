#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyWidget.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesStartGamePageWidget.h"
#include "MosesExperienceDefinition.generated.h"

class UMosesPawnData;
class UUserWidget;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** 이 Experience에서 활성화할 GameFeature Plugin 이름 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "GameFeatures")
	TArray<FString> GameFeaturesToEnable;

	/** 이 Experience에서 기본으로 사용할 PawnData */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TObjectPtr<const UMosesPawnData> DefaultPawnData;

	/** Start 단계에서 띄울 위젯 (Exp_Start에만 세팅) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UMosesStartGamePageWidget> StartWidgetClass;

	/** Lobby 단계에서 띄울 위젯 (Exp_Lobby에 세팅) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UMosesLobbyWidget> LobbyWidgetClass;

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
};
