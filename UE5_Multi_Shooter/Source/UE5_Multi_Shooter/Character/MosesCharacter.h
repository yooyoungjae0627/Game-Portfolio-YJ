#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MosesCharacter.generated.h"

/**
 * AMosesCharacter
 *
 * [기능]
 * - Player/Enemy가 공통으로 공유하는 "얇은 몸 베이스".
 *
 * [명세]
 * - 입력/카메라/픽업/GAS 초기화 같은 "플레이어 전용" 로직은 넣지 않는다.
 * - 공통 이동 기본값 세팅(선택)
 * - 공통 피격/사망 훅(표시/로그 정도) 제공
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMosesCharacter();

protected:
	virtual void BeginPlay() override;

protected:
	/** 공통 피격 훅(서버 판정 파이프라인에서 호출 가능) */
	virtual void HandleDamaged(float DamageAmount, AActor* DamageCauser);

	/** 공통 사망 훅(서버 판정 파이프라인에서 호출 가능) */
	virtual void HandleDeath(AActor* DeathCauser);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float DefaultWalkSpeed = 600.0f;
};
