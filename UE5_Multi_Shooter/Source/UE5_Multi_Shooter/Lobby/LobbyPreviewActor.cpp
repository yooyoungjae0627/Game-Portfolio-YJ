// LobbyPreviewActor.cpp
// ─────────────────────────────────────────────────────────────
// LobbyPreviewActor 구현
// - 메시 + AnimBP 교체만 수행
// - Idle 상태는 AnimBP 내부에서 자동 유지
// ─────────────────────────────────────────────────────────────

#include "LobbyPreviewActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"

ALobbyPreviewActor::ALobbyPreviewActor()
{
	// 프리뷰 액터는 Tick 불필요
	PrimaryActorTick.bCanEverTick = false;

	// 스켈레탈 메시 컴포넌트 생성
	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);

	// 프리뷰는 충돌/네트워크 불필요
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);

	// 항상 AnimBP 기반으로 동작
	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	// (선택) 그림자 필요 없으면 꺼서 비용 절감 가능
	// PreviewMesh->CastShadow = false;
}

void ALobbyPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// 레벨에 배치된 직후, 기본 타입 프리뷰 적용
	ApplyVisual(DefaultType);
}

void ALobbyPreviewActor::ApplyVisual(ECharacterType Type)
{
	const int32 Index = static_cast<int32>(Type);

	// 방어 코드: 배열 범위 체크
	if (!VisualSets.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyPreview][CL] ApplyVisual FAILED: Invalid Type=%d"), Index);
		return;
	}

	const FPreviewVisualSet& Visual = VisualSets[Index];

	// 방어 코드: 필수 에셋 체크
	if (!Visual.Mesh || !Visual.AnimClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyPreview][CL] ApplyVisual FAILED: Missing Asset Type=%d Mesh=%s Anim=%s"),
			Index,
			*GetNameSafe(Visual.Mesh),
			*GetNameSafe(Visual.AnimClass));
		return;
	}

	// 현재 타입 기록
	CurrentType = Type;

	// ─────────────────────────────────────────────
	// 핵심 로직
	// 1) SkeletalMesh 교체
	// 2) AnimBP 교체 → AnimBP 기본 상태(Idle) 자동 진입
	// ─────────────────────────────────────────────
	PreviewMesh->SetSkeletalMesh(Visual.Mesh);
	PreviewMesh->SetAnimInstanceClass(Visual.AnimClass);

	UE_LOG(LogTemp, Log,
		TEXT("[LobbyPreview][CL] ApplyVisual OK Type=%d Mesh=%s Anim=%s"),
		Index,
		*GetNameSafe(Visual.Mesh),
		*GetNameSafe(Visual.AnimClass));
}
