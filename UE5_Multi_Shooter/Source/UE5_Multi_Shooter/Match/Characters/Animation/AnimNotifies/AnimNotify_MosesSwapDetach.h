#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "AnimNotify_MosesSwapDetach.generated.h"

/**
 * Swap 몽타주에서 "기존 손 무기를 떼어 등으로 보내는" 타이밍을 처리한다.
 * - Dedicated Server에서는 코스메틱 처리 금지
 * - Pawn(Owner)에게만 전달
 */
UCLASS(meta = (DisplayName = "Moses Swap Detach"))
class UE5_MULTI_SHOOTER_API UAnimNotify_MosesSwapDetach : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
