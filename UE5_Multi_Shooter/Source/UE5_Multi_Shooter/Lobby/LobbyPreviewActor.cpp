// LobbyPreviewActor.cpp
// ─────────────────────────────────────────────────────────────
// LobbyPreviewActor 구현
// - 메시 + AnimBP 교체만 수행
// - Idle 상태는 AnimBP 내부에서 자동 유지
// ─────────────────────────────────────────────────────────────

#include "LobbyPreviewActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

ALobbyPreviewActor::ALobbyPreviewActor()
{
	// 프리뷰 액터는 Tick 불필요
	PrimaryActorTick.bCanEverTick = false;

	// ✅ 로컬 전용 액터 강제
	// - 절대 네트워크에 얹지 않는다.
	// - 서버/클라 모두 레벨에 존재할 수는 있어도 "동기화 대상"이 아니다.
	bReplicates = false;
	SetReplicateMovement(false);

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
	if (!IsVisualSetValid(Index))
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

	ApplyVisual_Internal(Visual, Index);
}

bool ALobbyPreviewActor::IsVisualSetValid(const int32 Index) const
{
	return VisualSets.IsValidIndex(Index);
}

void ALobbyPreviewActor::ApplyVisual_Internal(const FPreviewVisualSet& Visual, const int32 Index)
{
	// ─────────────────────────────────────────────
	// 핵심 로직 (안정성 보강 버전)
	// 1) SkeletalMesh 교체 (Reinit Pose)
	// 2) AnimBP 교체 → AnimBP 기본 상태(Idle) 자동 진입
	// 3) 필요 시 Anim 초기화 강제
	// ─────────────────────────────────────────────

	// 스켈레톤이 다른 Mesh로 교체될 수 있으므로 ReinitPose=true 권장
	PreviewMesh->SetSkeletalMesh(Visual.Mesh, true);

	// AnimBP 교체
	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	PreviewMesh->SetAnimInstanceClass(Visual.AnimClass);

	// 교체 직후 "첫 프레임 T-Pose/미갱신"을 줄이고 싶으면 Anim 초기화 강제
	PreviewMesh->InitAnim(true);

	UE_LOG(LogTemp, Log,
		TEXT("[LobbyPreview][CL] ApplyVisual OK Type=%d Mesh=%s Anim=%s"),
		Index,
		*GetNameSafe(Visual.Mesh),
		*GetNameSafe(Visual.AnimClass));
}
