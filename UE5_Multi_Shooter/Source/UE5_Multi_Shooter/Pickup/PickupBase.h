#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupBase.generated.h"

class APlayerCharacter;

/**
 * APickupBase
 *
 * [기능]
 * - 월드에 배치되는 "픽업" 오브젝트의 공통 베이스.
 * - 무기/탄약/힐 등 다양한 픽업이 이 클래스를 상속받아 "보상 적용"만 구현하면 된다.
 *
 * [핵심 철학(너 프로젝트 헌법)]
 * - 서버 권위 100%:
 *   - 클라이언트는 "요청"만 보낸다.
 *   - 서버가 거리/유효/중복/소비 여부를 판정하여 성공/실패를 확정한다.
 * - 원자성(Atomic):
 *   - 여러 명이 동시에 시도해도 서버에서 "딱 1명만 성공"이 보장된다.
 *
 * [명세]
 * - ServerTryConsume()는 서버에서만 의미가 있다.
 * - bProcessing/bConsumed로 동시 처리 잠금을 걸어 원자성을 보장한다.
 * - 성공 시 ApplyReward_Server()를 호출한다(자식에서 override).
 */
UCLASS()
class UE5_MULTI_SHOOTER_API APickupBase : public AActor
{
	GENERATED_BODY()

public:
	APickupBase();

	/** 서버 확정 소비 시도(원자성 보장) */
	UFUNCTION(BlueprintCallable, Category = "Moses|Pickup")
	void ServerTryConsume(APlayerCharacter* RequestingPawn);

protected:
	virtual void BeginPlay() override;

	/** 서버 전용: 보상 적용(자식 클래스가 override 해서 구현) */
	virtual void ApplyReward_Server(APlayerCharacter* RequestingPawn);

	/**
	 * [ADDED] 자식 픽업(Ammo/Weapon/Heal)이 override해서 실제 보상을 적용하는 서버 함수
	 * - true  : 적용 성공(이제 소비 확정 가능)
	 * - false : 적용 실패(소비 확정하면 안 됨)
	 *
	 * 주의: 이 함수는 서버에서만 호출된다.
	 */
	virtual bool Server_ApplyPickup(APawn* InstigatorPawn); // [ADDED]

private:
	bool CanConsume_Server(APlayerCharacter* RequestingPawn) const;

private:
	// ---------------------------
	// 튜닝
	// ---------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Pickup")
	float ConsumeDistance = 250.0f;

private:
	// ---------------------------
	// 서버 권위 상태
	// ---------------------------
	UPROPERTY(VisibleInstanceOnly, Category = "Moses|Pickup")
	bool bConsumed = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Moses|Pickup")
	bool bProcessing = false;
};
