#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesLobbyLocalPlayerSubsystem.generated.h"

class UMosesLobbyWidget;   // 구체 위젯 타입(룸ID 자동 입력 등) 호출하려고 필요
class UInputMappingContext;
class UUserWidget;

/**
 * UMosesLobbyLocalPlayerSubsystem
 *
 * 🎯 역할
 * - "로비 전용 UI + 입력(IMC) + 마우스 상태 + 입력 모드"를
 *   로컬 플레이어 단위로 관리하는 책임 클래스
 *
 * 📌 왜 LocalPlayerSubsystem인가?
 * - 클라이언트 전용 로직 (UI / 입력)
 * - Seamless Travel 이후에도 LocalPlayer는 유지됨
 * - GameMode / GameState와 명확히 책임 분리 가능
 *
 * 📌 설계 원칙
 * - GameFeatureAction: 활성/비활성 트리거만 담당
 * - Subsystem: 실제 생성 / 적용 / 해제 로직 담당
 *
 * → 로비 UI 문제 발생 시 이 클래스만 보면 됨
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesLobbyLocalPlayerSubsystem
	: public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Room 상태 변경 시 UI 갱신용 */
	void NotifyRoomStateChanged();

	/**
	 * GF_Lobby 활성 시 호출
	 *
	 * - Lobby UI 생성
	 * - Lobby Input Mapping Context 적용
	 * - 마우스 커서 + InputMode(GameAndUI) 설정
	 */
	void ActivateLobbyUI();

	/**
	 * GF_Lobby 비활성 시 호출
	 *
	 * - Lobby UI 제거
	 * - Lobby Input Mapping Context 제거
	 * - 입력 모드 GameOnly로 원복
	 */
	void DeactivateLobbyUI();

	// ✅ (PC ClientRPC -> Subsystem -> Widget) RoomId 자동 채우기 진입점
	void NotifyRoomCreated(const FGuid& NewRoomId);

private:
	/** IMC / Widget 에셋 로드 보장 */
	void EnsureAssetsLoaded();

	/** Lobby IMC 적용 (중복 방지 포함) */
	void AddLobbyMapping();

	/** Lobby IMC 제거 */
	void RemoveLobbyMapping();

private:
	/**
	 * 로비 전용 Input Mapping Context
	 *
	 * - StaticLoadObject로 직접 로드
	 * - 실패 시 명확한 로그 필수
	 */
	UPROPERTY()
	TObjectPtr<UInputMappingContext> IMC_Lobby = nullptr;

	/**
	 * IMC가 이미 Add 되었는지 여부
	 *
	 * - GameFeature 중복 활성
	 * - ActivateLobbyUI 중복 호출
	 * → 중복 Add 방지용
	 */
	bool bLobbyMappingAdded = false;

	/**
	 * 에셋 로드 시도 여부 캐싱
	 *
	 * - StaticLoad 실패 시
	 *   매번 Error 로그가 찍히는 것 방지
	 * - 최초 실패만 Error, 이후는 Skip
	 */
	bool bTriedLoadIMC = false;
	bool bTriedLoadWidgetClass = false;

private:
	/**
	 * 현재 활성화된 Lobby UI
	 *
     * - WeakPtr 사용 이유:
     *   · 레벨 이동 / GC / 외부 Remove 대비
     *   · 유효성 체크 후 안전 접근
     */

	UPROPERTY()
	TWeakObjectPtr<UMosesLobbyWidget> LobbyWidget;  // ✅ 구체 타입으로 저장

	UPROPERTY()
	TSubclassOf<UUserWidget> LobbyWidgetClass = nullptr; // ✅ cpp에서 쓰는 그 변수, 여기 있어야 함

};
