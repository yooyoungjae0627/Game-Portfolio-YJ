#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesStartGamePageWidget.generated.h"

class UEditableText;
class UButton;
class AMosesPlayerController;

/**
 * StartGameLevel UI
 * - 닉네임 입력 후 "입장"
 * - 서버에 닉 저장 → LobbyLevel로 ClientTravel
 *
 * 왜 Travel?
 * - 로비/매치처럼 GameMode/GameState 규칙이 바뀌는 구간은
 *   Streaming보다 Travel이 안전하고, 멀티도 정석이다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesStartGamePageWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void OnClicked_Enter();

private:
	FString GetNicknameText() const;
	AMosesPlayerController* GetMosesPC() const;

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableText> ET_Nickname = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> BTN_Enter = nullptr;
};
