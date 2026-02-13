// ============================================================================
// UE5_Multi_Shooter/Lobby/NPC/LobbyPreviewActor.cpp  (FULL - REORDERED)
// - ctor → BeginPlay → Public API → validation → internal apply
// ============================================================================

#include "LobbyPreviewActor.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

ALobbyPreviewActor::ALobbyPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 로컬 전용 액터 (네트워크 비동기화)
	bReplicates = false;
	SetReplicateMovement(false);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);

	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);

	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
}

void ALobbyPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyVisual(DefaultType);
}

// ============================================================================
// Public
// ============================================================================

void ALobbyPreviewActor::ApplyVisual(ECharacterType Type)
{
	const int32 Index = static_cast<int32>(Type);

	if (!IsVisualSetValid(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPreview][CL] ApplyVisual FAILED: Invalid Type=%d"), Index);
		return;
	}

	const FPreviewVisualSet& Visual = VisualSets[Index];

	if (!Visual.Mesh || !Visual.AnimClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyPreview][CL] ApplyVisual FAILED: Missing Asset Type=%d Mesh=%s Anim=%s"),
			Index,
			*GetNameSafe(Visual.Mesh),
			*GetNameSafe(Visual.AnimClass));
		return;
	}

	CurrentType = Type;
	ApplyVisual_Internal(Visual, Index);
}

// ============================================================================
// Internal
// ============================================================================

bool ALobbyPreviewActor::IsVisualSetValid(const int32 Index) const
{
	return VisualSets.IsValidIndex(Index);
}

void ALobbyPreviewActor::ApplyVisual_Internal(const FPreviewVisualSet& Visual, const int32 Index)
{
	PreviewMesh->SetSkeletalMesh(Visual.Mesh, /*bReinitPose*/ true);

	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	PreviewMesh->SetAnimInstanceClass(Visual.AnimClass);

	PreviewMesh->InitAnim(true);

	UE_LOG(LogTemp, Log, TEXT("[LobbyPreview][CL] ApplyVisual OK Type=%d Mesh=%s Anim=%s"),
		Index,
		*GetNameSafe(Visual.Mesh),
		*GetNameSafe(Visual.AnimClass));
}
