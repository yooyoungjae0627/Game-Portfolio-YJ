// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MosesGameModeBase.generated.h"

//class UMosesExperienceDefinition;
class UMosesPawnData;

/**
 * GameModeBase (서버 전용 컨트롤 타워)
 * - 매치 규칙/스폰 흐름/Experience 선택 및 로딩 시작을 담당
 *
 * 핵심 정책:
 * - Experience 로딩 전에는 스폰을 막는다.
 * - Experience 로딩 완료 이벤트가 오면 그때 RestartPlayer로 스폰한다.
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

	/** Experience 로딩 완료 전이면 스폰(Super) 보류 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) final;

	/** Defer Spawn으로 PawnData를 먼저 주입한 뒤 FinishSpawning */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) final;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	// ✅ 추가 추적 포인트(강추)
	virtual void RestartPlayer(AController* NewPlayer) override;
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
	//bool IsExperienceLoaded() const;

	/** Experience 로딩 완료 콜백 → Pawn 없는 플레이어 RestartPlayer */
	//void OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience);

};
