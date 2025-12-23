// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MosesPlayerState.generated.h"

class UMosesPawnData;
class UMosesExperienceDefinition;

/**
 * PlayerState
 * - 플레이어별 PawnData 저장소
 * - Experience 로딩 완료 시점에 PawnData를 확정/저장한다.
 */

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	/** ExperienceManager의 로딩 완료 이벤트를 등록하는 타이밍 */
	virtual void PostInitializeComponents() final;

	/** PlayerState가 보관 중인 PawnData 조회 */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

private:
	/** Experience 로딩 완료 콜백 → PawnData 확정/저장 */
	void OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience);

	/** PawnData 최초 1회 세팅(중복 세팅 방지) */
	void SetPawnData(const UMosesPawnData* InPawnData);

private:
	/** 플레이어별 PawnData(Experience 기본값 또는 커스텀 값) */
	UPROPERTY()
	TObjectPtr<const UMosesPawnData> PawnData;
	
	
};
