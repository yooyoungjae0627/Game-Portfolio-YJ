// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"     // FPrimaryAssetId
#include "MosesGameModeBase.generated.h"

class UMosesExperienceDefinition;
class UMosesPawnData;

/**
 * GameModeBase (서버 전용 컨트롤 타워)
 *
 * 책임:
 * - Experience 선택/로딩 시작
 * - Experience READY 이후에만 스폰 허용 (SpawnGate)
 * - 컨트롤러별 PawnData/PawnClass 결정 지원
 *
 * 핵심 정책(= 검증 포인트):
 * 1) READY 전: HandleStartingNewPlayer에서 스폰을 막고(Pending 큐잉) 리턴
 * 2) READY 후: Pending 플레이어들을 Flush하여 Super::HandleStartingNewPlayer로 정상 스폰 흐름 재개
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMosesGameModeBase();

	/** 맵 로드 직후 호출(OptionsString 확보 가능) → Experience 선택은 NextTick으로 미룸 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** GameState 생성 이후 → ExperienceManager의 로딩 완료 콜백 등록 */
	virtual void InitGameState() final;

	/** 컨트롤러별 PawnClass 결정(PawnData의 PawnClass 우선) */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) final;

	/**
	 * SpawnGate 핵심:
	 * - READY 전이면 Super 호출을 막고 PendingStartPlayers에 큐잉한다.
	 * - READY 후에는 Super::HandleStartingNewPlayer가 호출되도록 한다(Flush에서 호출).
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) final;

	/** Defer Spawn으로 PawnData를 먼저 주입한 뒤 FinishSpawning */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) final;

	/** 접속 시점 추적용(로그/디버깅) */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/**
	 * RestartPlayer는 최종 스폰 지점.
	 * - 안전장치: READY 전이면 호출되어도 스폰하지 않고 리턴(정책 강제)
	 */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** 접속 종료 추적용(바로 튕김/Seamless Travel 문제 디버깅에 유용) */
	virtual void Logout(AController* Exiting) override;

	/** 스폰 포인트/스폰 마무리 추적용 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

	/** PawnData 결정(PS 우선, 없으면 Experience DefaultPawnData) */
	const UMosesPawnData* GetPawnDataForController(const AController* InController) const;

private:
	/** NextTick에서 Experience 결정(OptionsString의 ?Experience=...) */
	void HandleMatchAssignmentIfNotExpectingOne();

	/** ExperienceId가 정해지면 ExperienceManager에게 로딩 지시 */
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId);

	/** Experience 로딩 완료 여부(스폰 게이트) */
	bool IsExperienceLoaded() const;

	/**
	 * Experience READY 콜백
	 * - [EXP][READY] 로그가 찍히는 지점
	 * - 여기서 SpawnGate를 해제하고 Pending 플레이어를 Flush한다.
	 */
	void OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience);

	/** Flush 중 재진입 방지(READY 직후 여러 이벤트로 중복 호출되는 경우 방어) */
	bool bFlushingPendingPlayers = false;

	/** READY 이후 NextTick에 Pending 플레이어들을 Super::HandleStartingNewPlayer로 넘긴다 */
	void FlushPendingPlayers();

	/** Experience READY → SpawnGate 해제(NextTick Flush 예약) */
	void OnExperienceReady_SpawnGateRelease();

	// MosesGameModeBase.h (ADD)

protected:
	/** DoD: Experience READY 이후(=안전 시점)에 파생 GM이 ROOM/PHASE를 확정하도록 훅 제공 */
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience);

	/** DoD: Experience Selected 로그를 1회만 찍기 위한 가드 */
	bool bDoD_ExperienceSelectedLogged = false;

	/** READY 전 접속한 플레이어들을 임시로 보관(스폰 대기열) */
	UPROPERTY()
	TArray<TWeakObjectPtr<APlayerController>> PendingStartPlayers;

};
