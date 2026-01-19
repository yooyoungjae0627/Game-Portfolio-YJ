#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "EnemyCharacter.generated.h"

/**
 * AEnemyCharacter
 *
 * [기능]
 * - Enemy 전용 Pawn.
 *
 * [명세]
 * - 입력/카메라/픽업/스프린트 로직 없음.
 * - AI/피격/사망 등은 이후 확장.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AEnemyCharacter : public AMosesCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

protected:
	virtual void BeginPlay() override;
};
