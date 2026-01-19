#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MosesCharacter.generated.h"

/**
 * AMosesCharacter
 *
 * [역할]
 * - Player/Enemy 공통 "얇은 Body".
 * - 입력/카메라/픽업/전투 상태(Ammo/Slot) 같은 Player 전용 로직은 절대 넣지 않는다.
 *
 * [GAS 정책]
 * - SSOT: PlayerState가 ASC Owner (OwnerActor)
 * - Pawn은 AvatarActor
 * - 따라서 Pawn이 생기거나(Spawn) 소유되거나(Possess) PlayerState가 연결되는 타이밍마다
 *   PlayerState::InitAbilityActorInfo(Owner=PS, Avatar=Pawn)이 보장되어야 한다.
 *
 * [핵심 훅]
 * - PossessedBy (Server): 서버에서 소유가 확정되는 순간
 * - OnRep_PlayerState (Client): 클라에서 PlayerState 복제가 도착한 순간
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMosesCharacter();

protected:
	virtual void BeginPlay() override;

	// [ADDED] GAS 안정화 훅
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:
	/** 공통 피격 훅(서버 판정 파이프라인에서 호출 가능) */
	virtual void HandleDamaged(float DamageAmount, AActor* DamageCauser);

	/** 공통 사망 훅(서버 판정 파이프라인에서 호출 가능) */
	virtual void HandleDeath(AActor* DeathCauser);

private:
	void TryInitASC_FromPawn(const TCHAR* Reason);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float DefaultWalkSpeed = 600.0f;
};
