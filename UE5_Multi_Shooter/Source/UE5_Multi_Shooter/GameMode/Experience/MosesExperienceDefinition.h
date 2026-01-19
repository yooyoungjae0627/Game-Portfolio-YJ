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
 * [한 줄 역할]
 * - “이번 플레이에서 무엇을 켤지”를 적어둔 **데이터 설계도(DataAsset)**
 *
 * [중요 원칙]
 * - 이 클래스는 "데이터"만 가진다.
 * - HUD 생성/입력 적용/Ability 부여 같은 "실제 적용"은 다른 레이어가 한다.
 *
 * [포트폴리오 관점]
 * - Warmup/Combat/Result처럼 페이즈가 바뀌면 Experience를 교체할 수 있다.
 * - Experience는 GF 목록 + PawnData + UI/Input/GAS Payload를 함께 정의한다.
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ----------------------------
	// UPrimaryDataAsset
	// ----------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif

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
	 * AbilitySet 경로(SoftObjectPath)
	 * - GAS가 이미 정리되어 있다면 “GF_Combat_GAS” 같은 GameFeature가 이 값을 보고 주입하도록 확장 가능
	 * - 지금 단계에서는 Registry에 “경로 보관”까지만 해도 포트폴리오 가치가 충분하다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience|PhasePayload|GAS")
	FSoftObjectPath AbilitySetPath;

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

	FSoftObjectPath GetAbilitySetPath() const { return AbilitySetPath; }

	// 스폰 단계에서 바로 쓰기 위한 "동기 로드 포인터 Getter"
	const UMosesPawnData* GetDefaultPawnDataLoaded() const; 

};
