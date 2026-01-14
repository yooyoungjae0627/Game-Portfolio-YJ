// MosesGestureDeckComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesGestureDeckComponent.generated.h"

class UAnimMontage;
class UAnimInstance;
class USkeletalMeshComponent;

/**
 * UMosesGestureDeckComponent
 *
 * - 제스처 몽타주 5종(GesturePool)을 덱(DeckOrder)으로 셔플해서 1회씩 사용
 * - 같은 제스처가 연속으로 나오지 않도록(Back-to-Back 방지)
 * - 소유 Actor(메타휴먼 BP 등)의 SkeletalMeshComponent에서 AnimMontage 재생
 *
 * 설계 포인트:
 * - 로컬 전용(네트워크/복제 없음)
 * - "답변 시작" 이벤트가 오면 PlayNextGesture()만 호출하면 됨
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesGestureDeckComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesGestureDeckComponent();

	// ---------------------------
	// Deck Core API
	// ---------------------------

	/** GesturePool 인덱스(0..N-1)로 덱 생성 + 셔플 */
	void InitDeck_Shuffle();

	/** 다음 인덱스 선택(Back-to-Back 방지 포함). 덱 소진 시 자동 재셔플 */
	int32 PickNextIndex_NoBackToBack();

	/** 특정 인덱스의 몽타주를 재생 */
	bool PlayPickedGesture(int32 PickedIndex);

	/** 다음 제스처(덱 기반)를 선택해서 재생 */
	bool PlayNextGesture();

	/** 현재 재생중인 제스처 몽타주를 정지(BlendOut) */
	void StopGesture(float BlendOutTime = 0.2f);

	// ---------------------------
	// Debug Getters (테스트 로그 확인용)
	// ---------------------------
	const TArray<int32>& GetDeckOrder() const { return DeckOrder; }
	int32 GetCursor() const { return Cursor; }
	int32 GetLastIndex() const { return LastIndex; }

protected:
	virtual void BeginPlay() override;

private:
	// ---------------------------
	// Internal Helpers
	// ---------------------------
	USkeletalMeshComponent* FindPlayableMesh() const;
	UAnimInstance* GetAnimInstanceFromMesh(USkeletalMeshComponent* Mesh) const;
	void LogDeckOrder(const TCHAR* Prefix) const;

private:
	// =========================================================
	// Editor Config
	// =========================================================

	/** ✅ 5개 몽타주를 넣는다(필수) */
	UPROPERTY(EditAnywhere, Category="Gesture|Config")
	TArray<TObjectPtr<UAnimMontage>> GesturePool;

	/**
	 * 메타휴먼 BP는 SkeletalMeshComponent가 여러 개일 수 있음.
	 * - 비어있으면: Owner의 첫 SkeletalMeshComponent를 사용
	 * - 이름 지정하면: 해당 이름의 SkeletalMeshComponent로 고정
	 */
	UPROPERTY(EditAnywhere, Category="Gesture|Config")
	FName PreferredSkeletalMeshComponentName = NAME_None;

	/** 같은 제스처 연속 방지(기본 true) */
	UPROPERTY(EditAnywhere, Category="Gesture|Config")
	bool bNoBackToBack = true;

	// =========================================================
	// Runtime State (Deck)
	// =========================================================

	/** 덱에 들어있는 GesturePool 인덱스 순서(셔플된 상태) */
	UPROPERTY(Transient)
	TArray<int32> DeckOrder;

	/** 덱 커서(0..DeckOrder.Num()) */
	UPROPERTY(Transient)
	int32 Cursor = 0;

	/** 직전에 재생했던 GesturePool 인덱스 */
	UPROPERTY(Transient)
	int32 LastIndex = INDEX_NONE;

	/** 재생에 사용한 AnimInstance 캐시(Stop 용도) */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

	/** 마지막으로 재생한 몽타주 캐시(Stop/중복 재생 정리 용도) */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimMontage> LastPlayedMontage;
};
