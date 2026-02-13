// ============================================================================
// UE5_Multi_Shooter/MosesGameModeBase.h
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"

#include "MosesGameModeBase.generated.h"

class UMosesExperienceDefinition;
class UMosesPawnData;

/**
 * AMosesGameModeBase
 *
 * 서버 전용 컨트롤 타워(GameMode).
 *
 * 주요 책임
 * - Experience 선택/로딩 시작 및 로딩 완료 이벤트 수신
 * - Experience READY 이전 스폰 차단(SpawnGate) 및 READY 이후 대기 플레이어 Flush
 * - 컨트롤러별 PawnData/PawnClass 결정 지원
 *
 * 핵심 정책(검증 포인트)
 * 1) READY 전: HandleStartingNewPlayer에서 스폰을 막고 PendingStartPlayers에 보관한다.
 * 2) READY 후: PendingStartPlayers를 Flush하여 Super::HandleStartingNewPlayer 흐름으로 정상 스폰을 재개한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

protected:
	AMosesGameModeBase();

	/** 맵 로드 직후 호출. OptionsString 확보 이후 Experience 선택은 NextTick으로 미룬다. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** GameState 생성 이후 ExperienceManager 로딩 콜백을 등록한다. */
	virtual void InitGameState() final;

	/** 접속 직후 호출. 접속/PS 상태 추적 로그용. */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 컨트롤러별 PawnClass 결정. PawnData의 PawnClass가 우선이며 없으면 Super를 따른다. */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/**
	 * SpawnGate 핵심.
	 * - Experience READY 전이면 Super를 호출하지 않고 PendingStartPlayers에 큐잉한다.
	 * - READY 후에는 FlushPendingPlayers에서 Super를 호출해 정상 스폰 흐름을 탄다.
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) final;

	/**
	 * Defer Spawn을 사용해 PawnData를 BeginPlay 이전에 주입한다.
	 * - SpawnActor(bDeferConstruction=true)
	 * - PawnExtensionComponent->SetPawnData()
	 * - FinishSpawning()
	 */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) final;

	/**
	 * 최종 스폰 지점.
	 * - 안전장치: READY 전이면 호출되더라도 스폰하지 않고 리턴한다.
	 */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** 접속 종료 추적. */
	virtual void Logout(AController* Exiting) override;

	/** PlayerStart 선택/스폰 마무리 추적. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

public:
	/** 파생 GameMode에서 Lobby로 돌아가는 ServerTravel을 구현한다. */
	virtual void ServerTravelToLobby();

	/** PawnData 결정. 우선순위: PlayerState > Experience DefaultPawnData. */
	const UMosesPawnData* GetPawnDataForController(const AController* InController) const;

	/** NetConnection으로부터 접속 주소(ip:port)를 문자열로 얻는다. */
	FString GetConnAddr(APlayerController* PC);

protected:
	/** Experience READY 이후 파생 GM이 ROOM/PHASE 등을 확정하도록 제공하는 훅. */
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience);

private:
	/** NextTick에서 Experience를 결정한다. (?Experience=...) 옵션 또는 맵 매핑 기반. */
	void HandleMatchAssignmentIfNotExpectingOne();

	/** ExperienceId가 정해지면 ExperienceManager에 로딩을 지시한다. */
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId);

	/** Experience 로딩 완료 여부(SpawnGate 판단). */
	bool IsExperienceLoaded() const;

	/** Experience 로딩 완료 콜백. SpawnGate 해제 및 Pending 플레이어 Flush 예약. */
	void OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience);

	/** READY 이후 NextTick에 Pending 플레이어들을 Super::HandleStartingNewPlayer로 넘긴다. */
	void FlushPendingPlayers();

	/** Experience READY 시점에 SpawnGate 해제(NextTick Flush 예약). */
	void OnExperienceReady_SpawnGateRelease();

private:
	/** READY 전 접속한 플레이어들을 임시 보관(스폰 대기열). */
	UPROPERTY()
	TArray<TWeakObjectPtr<APlayerController>> PendingStartPlayers;

	/** Flush 중 재진입 방지 가드. */
	bool bFlushingPendingPlayers = false;

	/** Experience Selected DoD 로그 1회만 출력하기 위한 가드. */
	bool bDoD_ExperienceSelectedLogged = false;
};
