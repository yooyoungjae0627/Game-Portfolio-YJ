#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MosesCrosshairWidget.generated.h"

class UImage;

/**
 * UMosesCrosshairWidget
 *
 * [역할]
 * - 정지/이동 상태에 따라 크로스헤어 간격을 수렴/확산시키는 "표시 전용" 위젯.
 *
 * [규칙]
 * - UMG Tick 금지
 * - UMG Designer Binding 금지
 * - HUD가 Timer로 SpreadFactor(0~1)을 계산해서 SetSpreadFactor로만 전달한다.
 */
UCLASS(Abstract)
class UE5_MULTI_SHOOTER_API UMosesCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesCrosshairWidget(const FObjectInitializer& ObjectInitializer);

public:
	/** SpreadFactor(0=수렴, 1=확산) */
	void SetSpreadFactor(float InSpreadFactor01);

protected:
	virtual void NativeOnInitialized() override;

private:
	void CacheBaseOffsetsIfNeeded();
	void ApplyOffset(UImage* Image, const FVector2D& BasePos, const FVector2D& AddPos);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Crosshair_Up = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Crosshair_Down = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Crosshair_Left = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Crosshair_Right = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Crosshair_CenterDot = nullptr;

private:
	/** SpreadFactor=0일 때 추가 간격(픽셀) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Crosshair", meta = (ClampMin = "0.0"))
	float MinSpreadPixels = 2.0f;

	/** SpreadFactor=1일 때 추가 간격(픽셀) */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Crosshair", meta = (ClampMin = "0.0"))
	float MaxSpreadPixels = 18.0f;

private:
	bool bCachedBase = false;

	FVector2D BaseUp = FVector2D::ZeroVector;
	FVector2D BaseDown = FVector2D::ZeroVector;
	FVector2D BaseLeft = FVector2D::ZeroVector;
	FVector2D BaseRight = FVector2D::ZeroVector;

	float CachedSpread = -1.0f;
};
