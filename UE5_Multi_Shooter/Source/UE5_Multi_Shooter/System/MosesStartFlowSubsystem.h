#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesStartFlowSubsystem.generated.h"

class UMosesUserSessionSubsystem;
class UUserWidget;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesStartFlowSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Start UI에서 Enter 치면 이 함수 호출하게 연결
	UFUNCTION(BlueprintCallable, Category="Moses|StartFlow")
	void SubmitNicknameAndEnterLobby(const FString& Nickname);

protected:
	void TryShowStartUI();

	// BP에서 Start 위젯 클래스를 지정해라
	UPROPERTY(EditAnywhere, Category="Moses|StartFlow")
	TSoftClassPtr<UUserWidget> StartWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> StartWidget;
};
