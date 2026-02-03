#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MosesStartGamePageWidget.generated.h"

class UEditableText;
class UButton;
class AMosesPlayerController;

/**
 * UMosesStartGamePageWidget
 *
 * - StartLevel에서 표시되는 시작/로그인 UI 위젯
 * - 요구사항:
 *   1) 이 위젯이 활성인 동안 마우스 커서는 항상 표시되어야 한다.
 *   2) 입력은 UI 중심으로 동작(닉네임 입력/버튼 클릭)
 *   3) 버튼 클릭 시: 로컬 닉네임 저장(LocalPlayerSubsystem) + 서버 EnterLobby 요청(Server RPC)
 *
 * 핵심 포인트:
 * - StartLevel에서 위젯 생성 직후에는 뷰포트/슬레이트 포커스가 아직 잡히지 않아
 *   커서가 "클릭 한 번 해야" 뜨는 경우가 있다.
 * - 따라서 NativeConstruct에서 1회 적용 + 다음 프레임에 1회 재적용을 수행한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesStartGamePageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMosesStartGamePageWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeConstruct() override;

private:
	// ---------------------------
	// UI Event
	// ---------------------------
	UFUNCTION()
	void OnClicked_Enter();

	// ---------------------------
	// Cursor / InputMode
	// ---------------------------
	void ApplyCursorAndFocus();                 // [ADD] 즉시 1회 적용
	UFUNCTION() void ApplyCursorAndFocus_NextTick(); // [ADD] 다음 프레임에 재적용 (포커스 문제 해결)

	// ---------------------------
	// Helpers
	// ---------------------------
	FString GetNicknameText() const;
	AMosesPlayerController* GetMosesPC() const;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> ET_Nickname;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Enter;

private:
	bool bAppliedInputMode = false;
};
