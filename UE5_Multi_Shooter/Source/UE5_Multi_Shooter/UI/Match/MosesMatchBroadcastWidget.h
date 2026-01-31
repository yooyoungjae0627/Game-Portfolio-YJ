// ============================================================================
// MosesMatchBroadcastWidget.h (FULL)
// ----------------------------------------------------------------------------
// - GameState의 MosesBroadcastComponent RepNotify->Delegate를 받아
//   방송 메시지를 HUD에 출력한다.
// - 자동 숨김은 로컬 타이머로 처리한다(게임 상태 변경 아님).
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesMatchBroadcastWidget.generated.h"

class UTextBlock;
class UMosesBroadcastComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesMatchBroadcastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UMosesBroadcastComponent* ResolveBroadcastComponent() const;

	void HandleBroadcastChanged(const FString& Message, float DisplaySeconds);
	void HideSelf();

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Broadcast;

	UPROPERTY(Transient)
	TObjectPtr<UMosesBroadcastComponent> CachedBroadcast;

	FTimerHandle TimerHandle_AutoHide;
};
