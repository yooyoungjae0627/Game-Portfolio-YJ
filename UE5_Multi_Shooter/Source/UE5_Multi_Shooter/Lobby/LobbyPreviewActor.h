// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "LobbyPreviewActor.generated.h"

// LobbyPreviewActor.h
// ─────────────────────────────────────────────────────────────
// 로비 캐릭터 선택 화면에서 사용하는 "프리뷰 전용 액터"
//
// 목적:
// - 레벨에 1개 고정 배치
// - 서버/네트워크/입력/카메라 로직 ❌
// - 메시 + AnimBP 교체만으로 "항상 Idle" 상태 보장
// - 스켈레톤이 서로 다른 캐릭터 타입을 안전하게 교체
//
// 핵심 원칙:
// - PlayAnimation 사용 ❌
// - AnimBP 기본 상태 = Idle
// - SetSkeletalMesh + SetAnimInstanceClass 만 사용
// ─────────────────────────────────────────────────────────────

/**
 * 캐릭터 선택 타입
 * - UI의 ← / → 인덱스와 1:1 매핑
 * - Count 는 항상 마지막
 */
UENUM(BlueprintType)
enum class ECharacterType : uint8
{
	TypeA UMETA(DisplayName = "TypeA"),
	TypeB UMETA(DisplayName = "TypeB"),
	Count UMETA(Hidden)
};

/**
 * 캐릭터 프리뷰에 필요한 "외형 세트"
 *
 * 주의:
 * - 스켈레톤이 다르면 AnimBP도 반드시 다르게!
 * - AnimBP 내부 기본 상태는 Idle 이어야 함
 */
USTRUCT(BlueprintType)
struct FPreviewVisualSet
{
	GENERATED_BODY()

	/** 프리뷰에 표시할 스켈레탈 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

	/** 해당 메시/스켈레톤 전용 Idle AnimBP */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TSubclassOf<UAnimInstance> AnimClass = nullptr;
};

/**
 * ALobbyPreviewActor
 *
 * - 로비 레벨에 1개만 존재하는 프리뷰 전용 액터
 * - 입력 / Possess / Replication / Camera 와 완전히 분리
 * - UI에서 로컬로만 ApplyVisual 호출
 */
UCLASS()
class UE5_MULTI_SHOOTER_API ALobbyPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ALobbyPreviewActor();

	/**
	 * 캐릭터 타입에 맞는 메시 + AnimBP 적용
	 * - AnimBP 기본 상태가 Idle 이므로 별도 재생 호출 ❌
	 * - 로컬 전용 (서버 호출 금지)
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Preview")
	void ApplyVisual(ECharacterType Type);

protected:
	/** BeginPlay 시 기본 프리뷰 타입 적용 */
	virtual void BeginPlay() override;

public:
	/** 실제 프리뷰에 사용되는 스켈레탈 메시 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;

	/**
	 * 타입별 외형 세트
	 * - VisualSets[(int32)Type] 방식으로 접근
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	TArray<FPreviewVisualSet> VisualSets;

	/** BeginPlay 시 자동으로 적용할 기본 타입 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	ECharacterType DefaultType = ECharacterType::TypeA;

	/** 현재 프리뷰 중인 타입 (디버그/로그용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Preview")
	ECharacterType CurrentType = ECharacterType::TypeA;
};

