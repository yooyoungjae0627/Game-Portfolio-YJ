#pragma once

#include "Blueprint/UserWidget.h"
#include "MosesLobbyWidget.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class AMosesPlayerController;

/**
 * UMosesLobbyWidget
 *
 * 책임(클라 UI):
 * - WBP_Lobby의 버튼/체크 이벤트를 C++에서 바인딩
 * - 입력값(룸ID 등) 검증 후 PlayerController 서버 RPC 호출
 * - 로비 상태(GameState Rep) 변경에 따라 Start 버튼 활성/비활성 갱신
 *
 * 주의:
 * - 이 클래스는 "클라 UI 로직"만 담당한다.
 * - 룸 생성/조인/레디/스타트의 실제 권한 검증은 서버(GameMode/GameState)에서 수행한다.
 * - BindWidget 변수는 WBP에서 "Is Variable" 체크 + 이름 일치가 필수.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 위젯 생성 직후 1회 호출: UI 이벤트 바인딩 + 초기 UI 상태 반영 */
	virtual void NativeConstruct() override;

	/** OwningPlayer를 MosesPlayerController로 캐스팅(서버 RPC 호출 진입점) */
	AMosesPlayerController* GetMosesPC() const;

	// ---- UI Event handlers ----
	UFUNCTION()
	void OnClicked_CreateRoom();

	UFUNCTION()
	void OnClicked_JoinRoom();

	UFUNCTION()
	void OnClicked_LeaveRoom();

	UFUNCTION()
	void OnClicked_StartGame();

	UFUNCTION()
	void OnReadyChanged(bool bIsChecked);

public:
	/** CreateRoom 성공 후(Subsystem/PC ClientRPC 등) RoomId를 텍스트박스에 자동 입력 */
	void SetRoomIdText(const FGuid& RoomId);

	/** Start 버튼 활성 조건 계산(Host + Room Full + All Ready) */
	bool CanStartGame() const;

	/** Start 버튼 상태 갱신(Rep 갱신/내 Ready 변경 시 호출) */
	void UpdateStartButton();

private:
	// ---- WBP BindWidget ----
	// WBP에서 "Is Variable" 체크 + 변수명 동일해야 한다.

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CreateRoom = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_JoinRoom = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_LeaveRoom = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_StartGame = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> CheckBox_Ready = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> TextBox_RoomId = nullptr;

private:
	/** 디버그용: CreateRoom MaxPlayers 기본값(테스트 중만 사용, 릴리즈는 서버 정책으로 고정 권장) */
	UPROPERTY(EditAnywhere, Category = "Lobby|Debug")
	int32 DebugMaxPlayers = 3;
};
