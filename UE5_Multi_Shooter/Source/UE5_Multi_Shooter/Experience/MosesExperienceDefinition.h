// ============================================================================
// MosesExperienceDefinition.h (CLEAN)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h" 
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "MosesExperienceDefinition.generated.h"

class UUserWidget;
class UInputMappingContext;
class UMosesPawnData;

/**
 * UMosesExperienceDefinition
 *
 * [정의]
 * - “로비/매치” 같은 단계에서 활성화해야 할 기능 세트를 묶는 DataAsset.
 *
 * [원칙]
 * - GameMode: 흐름(이동/스폰/타이밍)만 통제
 * - Experience: 해당 단계의 기능 구성(게임피처/UI/IMC/AbilitySet)을 정의
 * - 적용:
 *   - (클라) UI/IMC = ApplicatorSubsystem
 *   - (서버) AbilitySet = ExperienceManagerComponent
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMosesExperienceDefinition();

	// ----------------------------
	// UPrimaryDataAsset
	// ----------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif

public:
	// ----------------------------
	// Read-only Getter
	// ----------------------------
	const TArray<FString>& GetGameFeaturesToEnable() const { return GameFeaturesToEnable; }
	TSoftObjectPtr<const UMosesPawnData> GetDefaultPawnData() const { return DefaultPawnData; }

	TSoftClassPtr<UUserWidget> GetStartWidgetClass() const { return StartWidgetClass; }
	TSoftClassPtr<UUserWidget> GetHUDWidgetClass() const { return HUDWidgetClass; }

	TSoftObjectPtr<UInputMappingContext> GetInputMapping() const { return InputMapping; }
	int32 GetInputPriority() const { return InputPriority; }

	const FSoftObjectPath& GetAbilitySetPath() const { return AbilitySetPath; }

	/** 스폰 단계에서 바로 쓰기 위한 "동기 로드" 헬퍼 */
	const UMosesPawnData* GetDefaultPawnDataLoaded() const;

public:
	// ============================================================
	// 1) GameFeature
	// ============================================================
	UPROPERTY(EditDefaultsOnly, Category = "Experience|GameFeatures")
	TArray<FString> GameFeaturesToEnable;

	// ============================================================
	// 2) PawnData (Lyra Style)
	// ============================================================
	UPROPERTY(EditDefaultsOnly, Category = "Experience|Pawn")
	TSoftObjectPtr<const UMosesPawnData> DefaultPawnData;

	// ============================================================
	// 3) StartFlow (로비/시작 UI)
	// ============================================================
	UPROPERTY(EditDefaultsOnly, Category = "Experience|StartFlow")
	TSoftClassPtr<UUserWidget> StartWidgetClass;

	// ============================================================
	// 4) Phase Payload (HUD / Input / GAS)
	// ============================================================
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|UI")
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|Input")
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|Input")
	int32 InputPriority = 100;

	/**
	 * AbilitySetPath
	 * - 서버에서 ASC에 부여할 AbilitySet DataAsset 경로
	 * - 적용은 ExperienceManagerComponent가 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|GAS")
	FSoftObjectPath AbilitySetPath;

	// ============================================================
	// (Optional) Legacy / Debug paths
	// - 기존 프로젝트 호환용. 가능하면 위 SoftPtr을 정식 사용 권장.
	// ============================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Experience", meta = (ToolTip = "(Optional) URL 형태로 GF를 들고 싶을 때. 주로 로그/증명용"))
	TArray<FString> GameFeaturePluginURLs;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|UI", meta = (ToolTip = "(Optional) SoftClassPath 버전 HUD 경로. 가능하면 HUDWidgetClass를 정식 사용"))
	FSoftClassPath HUDWidgetClassPath;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Input", meta = (ToolTip = "(Optional) IMC 경로. 가능하면 InputMapping SoftPtr을 정식 사용"))
	FSoftObjectPath InputMappingContextPath;
};
