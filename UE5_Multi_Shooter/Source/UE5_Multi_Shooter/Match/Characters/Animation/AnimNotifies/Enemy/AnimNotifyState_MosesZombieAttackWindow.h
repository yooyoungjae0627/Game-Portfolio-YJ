#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Types/MosesZombieAttackTypes.h"

#include "AnimNotifyState_MosesZombieAttackWindow.generated.h"

/**
 * AnimNotifyState - Zombie melee attack window
 * - Montage 구간 동안만 서버에서 AttackHitBox Collision ON/OFF를 제어한다.
 * - 클라이언트에서도 노티파이가 호출되지만, 실제 판정은 서버만 수행(HasAuthority() Guard).
 */
UCLASS(meta=(DisplayName="Moses Zombie Attack Window"))
class UE5_MULTI_SHOOTER_API UAnimNotifyState_MosesZombieAttackWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_MosesZombieAttackWindow();

public:
	/** 이 윈도우에서 활성화할 손(좌/우/둘다) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moses|Zombie")
	EMosesZombieAttackHand AttackHand = EMosesZombieAttackHand::Right;

	/** (선택) 윈도우 시작 시 히트한 액터 기록을 초기화할지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moses|Zombie")
	bool bResetHitActorsOnBegin = true;

protected:
	//~UAnimNotifyState interface
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
