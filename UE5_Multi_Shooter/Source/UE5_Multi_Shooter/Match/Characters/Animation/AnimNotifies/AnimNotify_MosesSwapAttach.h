#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "AnimNotify_MosesSwapAttach.generated.h"

/**
 * Swap 몽타주에서 "새 무기를 손에 붙이는" 타이밍을 처리한다.
 * - Dedicated Server에서는 코스메틱 처리 금지
 * - Pawn(Owner)에게만 전달
 */
UCLASS(meta = (DisplayName = "Moses Swap Attach"))
class UE5_MULTI_SHOOTER_API UAnimNotify_MosesSwapAttach : public UAnimNotify
{
	GENERATED_BODY()

public:
	// ✅ New API (Deprecated 경고 제거)
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
