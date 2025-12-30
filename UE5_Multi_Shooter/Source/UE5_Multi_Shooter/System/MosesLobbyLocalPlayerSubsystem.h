#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;
class UInputMappingContext;
class UUserWidget;

/**
 * UMosesLobbyLocalPlayerSubsystem
 *
 * 책임(클라 전용):
 * - 로비 UI 생성/제거(뷰포트)
 * - 로비 입력 IMC Add/Remove
 * - 마우스 커서 / InputMode(GameAndUI ↔ GameOnly) 전환
 * - LobbyGameState Rep 갱신 시 UI Refresh 트리거
 *
 * 유지 조건:
 * - LocalPlayer 단위로 유지되므로 SeamlessTravel 왕복에서도 상태가 살아있다.
 *
 * 호출 주체:
 * - GameFeatureAction(활성/비활성 트리거) → Subsystem(실제 처리)
 * - GameState OnRep → LocalPlayerSubsystem.NotifyRoomStateChanged()
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** GameState RepRoomList 등 "로비 상태 변경" 시 UI 갱신 트리거 */
	void NotifyRoomStateChanged();

	/**
	 * GF_Lobby 활성 시 호출(클라 전용)
	 * - UI 생성 + AddToViewport
	 * - IMC 적용
	 * - InputMode(GameAndUI) + ShowMouseCursor 켬
	 */
	void ActivateLobbyUI();

	/**
	 * GF_Lobby 비활성 시 호출(클라 전용)
	 * - UI 제거
	 * - IMC 제거
	 * - InputMode(GameOnly) + ShowMouseCursor 끔
	 */
	void DeactivateLobbyUI();

	/** Room 생성 완료 후(PC ClientRPC 등) 위젯에 RoomId 자동 입력 */
	void NotifyRoomCreated(const FGuid& NewRoomId);

private:
	/** 로비 UI/입력에서 쓰는 에셋(IMC/WidgetClass) 로드 보장(1회 시도 캐시 포함) */
	void EnsureAssetsLoaded();

	/** 로비 IMC 적용(중복 Add 방지) */
	void AddLobbyMapping();

	/** 로비 IMC 제거(중복 Remove 방지) */
	void RemoveLobbyMapping();

private:
	/** 로비 전용 IMC 오브젝트(StaticLoadObject로 로드) */
	UPROPERTY()
	TObjectPtr<UInputMappingContext> IMC_Lobby = nullptr;

	/** IMC가 AddMappingContext 되었는지(중복 Add 방지 플래그) */
	bool bLobbyMappingAdded = false;

	/** 에셋 로드 실패 로그 스팸 방지용(최초 1회만 Error) */
	bool bTriedLoadIMC = false;
	bool bTriedLoadWidgetClass = false;

private:
	/** 현재 활성 Lobby UI (레벨 이동/GC 대비: WeakPtr) */
	UPROPERTY()
	TWeakObjectPtr<UMosesLobbyWidget> LobbyWidget;

	/** 생성할 Lobby 위젯 클래스(StaticLoadClass로 로드) */
	UPROPERTY()
	TSubclassOf<UUserWidget> LobbyWidgetClass = nullptr;
};
