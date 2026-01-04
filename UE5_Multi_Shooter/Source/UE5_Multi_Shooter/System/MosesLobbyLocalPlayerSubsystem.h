// MosesLobbyLocalPlayerSubsystem.h
#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;
class ALobbyPreviewActor;

enum class EMosesRoomJoinResult : uint8;

DECLARE_MULTICAST_DELEGATE(FOnLobbyPlayerStateChanged);
DECLARE_MULTICAST_DELEGATE(FOnLobbyRoomStateChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyJoinRoomResult, EMosesRoomJoinResult /*Result*/, const FGuid& /*RoomId*/);

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
	void ActivateLobbyUI();
	void DeactivateLobbyUI();

	// ---------------------------
	// Notify (PS/GS -> Subsystem)
	// ---------------------------
	void NotifyPlayerStateChanged();
	void NotifyRoomStateChanged();

	// ✅ NEW: Join 결과(성공/실패)를 UI로 전달
	void NotifyJoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId);

	// ---------------------------
	// Events (Widget bind)
	// ---------------------------
	FOnLobbyPlayerStateChanged& OnLobbyPlayerStateChanged() { return LobbyPlayerStateChangedEvent; }
	FOnLobbyRoomStateChanged& OnLobbyRoomStateChanged() { return LobbyRoomStateChangedEvent; }

	// ✅ NEW
	FOnLobbyJoinRoomResult& OnLobbyJoinRoomResult() { return LobbyJoinRoomResultEvent; }

	// ---------------------------
	// Lobby Preview (Public API - Widget에서 호출)
	// ---------------------------
	void RequestPrevCharacterPreview_LocalOnly();
	void RequestNextCharacterPreview_LocalOnly();

private:
	// ---------------------------
	// Internal helpers
	// ---------------------------
	bool IsLobbyWidgetValid() const;

	// ---------------------------
	// Lobby Preview (Local only)
	// ---------------------------
	void RefreshLobbyPreviewCharacter_LocalOnly();
	void ApplyPreviewBySelectedId_LocalOnly(int32 SelectedId);
	ALobbyPreviewActor* GetOrFindLobbyPreviewActor_LocalOnly();

	// ---------------------------
	// Player access helper
	// ---------------------------
	class AMosesPlayerController* GetMosesPC_LocalOnly() const;
	class AMosesPlayerState* GetMosesPS_LocalOnly() const;

private:
	// ---------------------------
	// State
	// ---------------------------
	UPROPERTY(Transient)
	TObjectPtr<UMosesLobbyWidget> LobbyWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ALobbyPreviewActor> CachedLobbyPreviewActor = nullptr;

	FOnLobbyPlayerStateChanged LobbyPlayerStateChangedEvent;
	FOnLobbyRoomStateChanged LobbyRoomStateChangedEvent;

	FOnLobbyJoinRoomResult LobbyJoinRoomResultEvent;

	// 개발자 주석:
	// - "프리뷰"는 로컬 연출이므로 서버/복제 없이 Local에서만 유지한다.
	int32 LocalPreviewSelectedId = 1;
};
