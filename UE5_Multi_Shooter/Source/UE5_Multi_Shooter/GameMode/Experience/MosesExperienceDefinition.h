#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "MosesExperienceDefinition.generated.h"

class UUserWidget;
class UInputMappingContext;
class UMosesPawnData;

/**
 * UMosesExperienceDefinition
 *
 * [정의]
 * - 현재 “게임 단계(로비/매치 등)”에서 활성화되어야 하는 기능 세트를 묶는 DataAsset.
 *
 * [원칙]
 * - GameMode는 “흐름(이동/스폰 통제)”만 결정한다.
 * - Experience는 “해당 단계에서 필요한 기능 세트”를 정의한다.
 * - 실제 적용은:
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

	// ---------------------------
	// Read-only Accessors
	// ---------------------------
public:
	const TArray<FString>& GetGameFeaturePluginURLs() const { return GameFeaturePluginURLs; }
	const FSoftClassPath& GetHUDWidgetClassPath() const { return HUDWidgetClassPath; }
	const FSoftObjectPath& GetInputMappingContextPath() const { return InputMappingContextPath; }
	const FSoftObjectPath& GetAbilitySetPath() const { return AbilitySetPath; }

public:
	// ----------------------------
	// Getter (외부는 읽기만)
	// ----------------------------
	const TArray<FString>& GetGameFeaturesToEnable() const { return GameFeaturesToEnable; }
	TSoftObjectPtr<const UMosesPawnData> GetDefaultPawnData() const { return DefaultPawnData; }

	TSoftClassPtr<UUserWidget> GetStartWidgetClass() const { return StartWidgetClass; }
	TSoftClassPtr<UUserWidget> GetHUDWidgetClass() const { return HUDWidgetClass; }

	TSoftObjectPtr<UInputMappingContext> GetInputMapping() const { return InputMapping; }
	int32 GetInputPriority() const { return InputPriority; }

	// 스폰 단계에서 바로 쓰기 위한 "동기 로드 포인터 Getter"
	const UMosesPawnData* GetDefaultPawnDataLoaded() const;

public:
	// ============================================================
	// 1) GameFeature
	// ============================================================

	/**
	 * 이 Experience에서 활성화할 GameFeature Plugin 이름 목록
	 * 예) "GF_Lobby_Code", "GF_Match_Code", "GF_Combat_UI" ...
	 *
	 * - ExperienceManager가 이 목록을 보고 GameFeatureSubsystem에 Load+Activate를 요청한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|GameFeatures")
	TArray<FString> GameFeaturesToEnable;

	// ============================================================
	// 2) PawnData (Lyra Style)
	// ============================================================

	/**
	 * Lyra 스타일 PawnData 주입용(Soft 권장)
	 * - Experience는 "이번 판에서 어떤 PawnData를 사용할지"만 정의한다.
	 * - 실제 스폰에서 적용은 GameMode/Spawn 시스템이 수행한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|Pawn")
	TSoftObjectPtr<const UMosesPawnData> DefaultPawnData;

	// ============================================================
	// 3) StartFlow 호환(로비/시작 UI)
	// ============================================================

	/**
	 * 로비/시작 화면에서 띄울 위젯(기존 StartFlowSubsystem 호환용)
	 * - 전투 HUD와는 별개 개념이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|StartFlow")
	TSoftClassPtr<UUserWidget> StartWidgetClass;

	// ============================================================
	// 4) Phase Payload (HUD / Input / GAS)
	// ============================================================

	/** HUD 위젯(SoftClass) - 로컬 적용자가 READY 시점에 생성해서 붙인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|UI")
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	/** Input Mapping Context (SoftObject) - 로컬 적용자가 READY 시점에 적용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|Input")
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	/** IMC 우선순위(큰 값이 우선) */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|Input")
	int32 InputPriority = 100;

	/**
	 * GameFeaturePluginURLs
	 * - GameFeature 플러그인을 “URL 형태”로 보관할 수 있다.
	 * -“켜졌다/꺼졌다” 로그 증명만으로도 충분.
	 * - 실제 로딩은 너 프로젝트 ExperienceManager/Registry 정책에 따라 확장 가능.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Experience")
	TArray<FString> GameFeaturePluginURLs;

	/**
	 * HUDWidgetClassPath
	 * - 적용(생성/AddToViewport)은 ApplicatorSubsystem가 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|UI")
	FSoftClassPath HUDWidgetClassPath;

	/**
	 * InputMappingContextPath
	 * - EnhancedInput IMC 경로
	 * - 적용은 ApplicatorSubsystem가 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Input")
	FSoftObjectPath InputMappingContextPath;

	/**
	 * AbilitySetPath
	 * - 서버에서 ASC에 부여할 AbilitySet DataAsset 경로
	 * - 적용은 ExperienceManagerComponent가 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|GAS")
	FSoftObjectPath AbilitySetPath;

};
