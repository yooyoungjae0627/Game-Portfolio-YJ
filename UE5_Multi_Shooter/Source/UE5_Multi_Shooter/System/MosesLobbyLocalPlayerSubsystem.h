#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;

DECLARE_MULTICAST_DELEGATE(FOnLobbyPlayerStateChanged);
DECLARE_MULTICAST_DELEGATE(FOnLobbyRoomStateChanged);

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

protected:
	// ---------------------------
	// Engine
	// ---------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// ---------------------------
	// UI control (Public API)
	// ---------------------------

	/** 개발자 메모)
	 *  외부(PC 등)에서 로비 UI를 켤 수 있어야 하므로 public으로 둔다.
	 *  내부에서는 중복 생성 방지 및 안전 체크를 수행한다.
	 */
	void ActivateLobbyUI();

	/** 로비 UI 제거 */
	void DeactivateLobbyUI();

	// ---------------------------
	// Notify (PS/GS -> Subsystem)
	// ---------------------------

	/** 개발자 메모)
	 *  PlayerState의 RepNotify / 변경 타이밍에서 호출되는 "UI 갱신 트리거".
	 *  실제 갱신 로직은 위젯 또는 바인딩된 리스너가 수행한다.
	 */
	void NotifyPlayerStateChanged();

	/** GameState의 방 상태 변경(룸 목록/멤버/Ready 등) 알림 */
	void NotifyRoomStateChanged();

	// ---------------------------
	// Events (Widget bind)
	// ---------------------------
	FOnLobbyPlayerStateChanged& OnLobbyPlayerStateChanged() { return LobbyPlayerStateChangedEvent; }
	FOnLobbyRoomStateChanged& OnLobbyRoomStateChanged() { return LobbyRoomStateChangedEvent; }

private:
	// ---------------------------
	// Internal helpers
	// ---------------------------
	bool IsLobbyWidgetValid() const;

private:
	// ---------------------------
	// State
	// ---------------------------
	UPROPERTY()
	TObjectPtr<UMosesLobbyWidget> LobbyWidget = nullptr;

	FOnLobbyPlayerStateChanged LobbyPlayerStateChangedEvent;
	FOnLobbyRoomStateChanged LobbyRoomStateChangedEvent;
};
