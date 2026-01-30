#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchTypes.h"
#include "MosesMatchAnnouncementWidget.generated.h"

class UTextBlock;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesMatchAnnouncementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateAnnouncement(const FMosesAnnouncementState& State);

protected:
	virtual void NativeOnInitialized() override;

private:
	void SetActive(bool bActive);

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> AnnouncementText = nullptr;
};
