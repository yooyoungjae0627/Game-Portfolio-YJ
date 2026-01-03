#pragma once

#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesGameState.generated.h"

// < GameState는 게임 서버에 붙어 있는 상태 알림판 같은 클래스 >.
// 서버는 이 알림판에 “지금 로비야”, “누가 시작 버튼을 눌렀어”, 
//“지금 캐릭터를 만들어도 돼 ? ” 같은 걸 적어두고, 
// 모든 플레이어가 그걸 똑같이 보게 함.
// 중요한 점은 시작 버튼을 눌렀다고 바로 캐릭터가 생기지 않는다는 것.
// 시작 버튼은 그냥 “이제 시작할 준비가 됐어요”라고 서버에 알려주는 신호일 뿐이고, 
// 캐릭터가 실제로 생성되는지는 SpawnGate라는 문이 열렸을 때만 가능함.
// 이 문은 서버가 “게임 시작 후 레벨로 이동하고, 준비가 끝난 정확한 순간”에만 열어줌.
// 그래서 이 코드는 로비에서 실수로 캐릭터가 튀어나오거나, 시작 버튼을 여러 번 눌러서 게임이 꼬이는 걸 막기 위해, 
// 서버가 모든 상태를 한 곳에서 엄격하게 관리하도록 만든 코드.

class UMosesExperienceManagerComponent;

/**
 * 서버가 현재 "어디 단계에 있는지"를 표현하는 enum
 * GameMode가 아니라 GameState가 들고 있는 이유:
 * - 클라이언트도 이 값을 알아야 UI를 그릴 수 있기 때문
 */
UENUM(BlueprintType)
enum class EMosesServerPhase : uint8
{
	None  UMETA(DisplayName = "None"),
	Lobby UMETA(DisplayName = "Lobby"), // 로비 상태
	Match UMETA(DisplayName = "Match"), // 실제 게임 진행 상태
};

/**
 * AMosesGameState
 *
 * 한 문장 요약
 * → "서버의 현재 단계(Phase), Start 요청 여부, Spawn 가능 여부를
 *    단일 진실(Source of Truth)로 관리하는 상태판"
 *
 * 설계 원칙
 * 1. 서버만 상태를 변경한다 (Authority Only)
 * 2. 클라이언트는 복제된 값만 '본다'
 * 3. Ready ≠ Spawn (시작 요청과 스폰은 절대 같은 개념이 아님)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:
	AMosesGameState(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;

	/**
	 * Replication 설정
	 * → 어떤 변수를 클라이언트로 동기화할지 정의
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/**
	 * Phase가 바뀌었을 때 클라이언트에서 호출됨
	 * UI 갱신 용도
	 */
	UFUNCTION()
	void OnRep_CurrentPhase();

	/**
	 * SpawnGate 상태 변경 시 클라이언트 콜백
	 */
	UFUNCTION()
	void OnRep_SpawnGateOpen();

	/**
	 * Start 요청 상태 변경 시 클라이언트 콜백
	 */
	UFUNCTION()
	void OnRep_StartRequested();

public:
	/**
	 * 서버 전용
	 * 현재 서버 Phase를 확정한다
	 * 중복 변경 방지 / 서버 로그 기준 고정
	 */
	void ServerSetPhase(EMosesServerPhase NewPhase);

	/**
	 * Start 버튼 요청 "예약" 전용 함수
	 *
	 * ❌ Phase 변경 안 함
	 * ❌ Spawn 안 함
	 * ❌ ServerTravel 안 함
	 *
	 * 오직 "Start 요청이 있었다"는 사실만 서버에 기록
	 */
	void ServerRequestStart_ReserveOnly(const FGuid& RequesterPid);

	/**
	 * SpawnGate를 닫는다
	 * 이미 닫혀 있어도 로그를 남겨 판단 근거를 확보
	 */
	void ServerCloseSpawnGate();

	// 조회 함수들 (읽기 전용)
	bool IsStartRequested() const;
	bool IsSpawnGateOpen() const;
	EMosesServerPhase GetCurrentPhase() const;

protected:
	/**
	 * Experience 로딩 / Ready 상태를 관리하는 컴포넌트
	 * Phase 1 이후 본격 활용
	 */
	UPROPERTY()
	TObjectPtr<UMosesExperienceManagerComponent> ExperienceManagerComponent;

	/**
	 * 누가 Start를 눌렀는지 기록용
	 * 디버그 / 로그 / 추후 권한 검증용
	 */
	UPROPERTY(Replicated)
	FGuid StartRequesterPid;

	// --------------------------------------------------
	// SpawnGate (핵심 개념)
	// --------------------------------------------------
	/**
	 * "지금 스폰이 발생해도 되는가?"
	 *
	 * ❗ 매우 중요
	 * - Start를 눌렀다고 스폰이 되면 안 된다
	 * - ServerTravel, 준비시간, Phase 전환 등
	 *   '정확한 타이밍'에만 열려야 한다
	 *
	 * Ready ≠ Spawn
	 */
	UPROPERTY(ReplicatedUsing = OnRep_SpawnGateOpen)
	bool bSpawnGateOpen = false;

	// --------------------------------------------------
	// StartRequested (핵심 개념)
	// --------------------------------------------------
	/**
	 * Start 버튼이 눌렸는지 여부
	 *
	 * 목적:
	 * - 중복 Start 요청 방지
	 * - 서버 단일 진실 유지
	 *
	 * Start = "예약"이지 "시작"이 아니다
	 */
	UPROPERTY(ReplicatedUsing = OnRep_StartRequested)
	bool bStartRequested = false;

	/**
	 * 현재 서버 Phase
	 * 서버가 공식적으로 들고 있는 상태
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	EMosesServerPhase CurrentPhase = EMosesServerPhase::None;
};
