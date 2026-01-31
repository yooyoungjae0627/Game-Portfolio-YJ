// ============================================================================
// MosesCrosshairWidget.h (FULL)
// ----------------------------------------------------------------------------
// - 로컬 코스메틱: Pawn 속도 기반으로 "SpreadFactor"를 계산해 크로스헤어 간격을 조절한다.
// - Tick 금지 원칙 준수: Timer(기본 0.05s)로 갱신한다.
// - 서버 판정(스프레드 적용)은 CombatComponent에서 별도로 수행한다.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesCrosshairWidget.generated.h"

class UImage;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void UpdateCrosshair_Local();
	float ComputeSpreadFactor_Local() const;

	void SetImageOffset(UImage* Img, float X, float Y) const;

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Top;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Bottom;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Left;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Img_Right;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair")
	float MinOffset = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair")
	float MaxOffset = 28.0f;

	UPROPERTY(EditDefaultsOnly, Category="Crosshair")
	float UpdateInterval = 0.05f;

	FTimerHandle TimerHandle_Update;
};
