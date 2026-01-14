// MosesGestureDeckComponent.cpp

#include "UE5_Multi_Shooter/Dialogue/Components/MosesGestureDeckComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UMosesGestureDeckComponent::UMosesGestureDeckComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesGestureDeckComponent::BeginPlay()
{
	Super::BeginPlay();

	// 게임 시작 시 덱 준비(테스트 설명서: InitDeck 셔플 로그 확인 가능)
	InitDeck_Shuffle();
}

void UMosesGestureDeckComponent::InitDeck_Shuffle()
{
	DeckOrder.Reset();

	// GesturePool에서 유효한 몽타주만 덱에 등록
	for (int32 i = 0; i < GesturePool.Num(); ++i)
	{
		if (GesturePool[i])
		{
			DeckOrder.Add(i);
		}
	}

	Cursor = 0;

	// Fisher-Yates shuffle
	if (DeckOrder.Num() > 1)
	{
		for (int32 i = DeckOrder.Num() - 1; i > 0; --i)
		{
			const int32 SwapIdx = FMath::RandRange(0, i);
			DeckOrder.Swap(i, SwapIdx);
		}
	}

	LogDeckOrder(TEXT("[GestureDeck] InitDeck_Shuffle"));
}

int32 UMosesGestureDeckComponent::PickNextIndex_NoBackToBack()
{
	if (DeckOrder.Num() <= 0)
	{
		// GesturePool이 비었거나 전부 null이면 여기로 떨어짐
		return INDEX_NONE;
	}

	// 덱 소진 시 재셔플(테스트 설명서: 5회 사용 후 재셔플 로그 확인)
	if (Cursor >= DeckOrder.Num())
	{
		InitDeck_Shuffle();
	}

	int32 Picked = DeckOrder.IsValidIndex(Cursor) ? DeckOrder[Cursor] : INDEX_NONE;

	// Back-to-Back 방지:
	// 현재 픽이 직전(LastIndex)과 같으면 다음 요소와 swap 시도
	if (bNoBackToBack && Picked != INDEX_NONE && Picked == LastIndex && DeckOrder.Num() > 1)
	{
		const int32 NextCursor = (Cursor + 1) % DeckOrder.Num();
		DeckOrder.Swap(Cursor, NextCursor);
		Picked = DeckOrder[Cursor];
	}

	++Cursor;
	return Picked;
}

bool UMosesGestureDeckComponent::PlayPickedGesture(int32 PickedIndex)
{
	if (!GesturePool.IsValidIndex(PickedIndex))
	{
		return false;
	}

	UAnimMontage* Montage = GesturePool[PickedIndex];
	if (!Montage)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = FindPlayableMesh();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GestureDeck] No SkeletalMeshComponent on Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	UAnimInstance* AnimInst = GetAnimInstanceFromMesh(Mesh);
	if (!AnimInst)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GestureDeck] No AnimInstance on Mesh=%s Owner=%s"),
			*GetNameSafe(Mesh), *GetNameSafe(GetOwner()));
		return false;
	}

	// 이전 몽타주가 재생 중이면 자연스럽게 stop 후 다음을 플레이
	if (LastPlayedMontage.IsValid() && AnimInst->Montage_IsPlaying(LastPlayedMontage.Get()))
	{
		AnimInst->Montage_Stop(0.15f, LastPlayedMontage.Get());
	}

	const float PlayedLen = AnimInst->Montage_Play(Montage, 1.0f);
	if (PlayedLen <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GestureDeck] Montage_Play failed. Montage=%s"), *GetNameSafe(Montage));
		return false;
	}

	// 상태 갱신(테스트 설명서: back-to-back 방지 검증에 사용)
	LastIndex = PickedIndex;
	CachedAnimInstance = AnimInst;
	LastPlayedMontage = Montage;

	UE_LOG(LogTemp, Log, TEXT("[GestureDeck] Play Picked=%d Montage=%s Cursor=%d"),
		PickedIndex, *GetNameSafe(Montage), Cursor);

	return true;
}

bool UMosesGestureDeckComponent::PlayNextGesture()
{
	const int32 Picked = PickNextIndex_NoBackToBack();
	if (Picked == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GestureDeck] PickNextIndex failed. Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	return PlayPickedGesture(Picked);
}

void UMosesGestureDeckComponent::StopGesture(float BlendOutTime)
{
	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	UAnimMontage* Montage = LastPlayedMontage.Get();

	if (AnimInst && Montage && AnimInst->Montage_IsPlaying(Montage))
	{
		AnimInst->Montage_Stop(BlendOutTime, Montage);
		UE_LOG(LogTemp, Log, TEXT("[GestureDeck] StopGesture BlendOut=%.2f Montage=%s"),
			BlendOutTime, *GetNameSafe(Montage));
	}
}

USkeletalMeshComponent* UMosesGestureDeckComponent::FindPlayableMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// 1) 이름 지정이 있으면 해당 MeshComponent를 우선 탐색
	if (PreferredSkeletalMeshComponentName != NAME_None)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Owner->GetComponents(Meshes);

		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetFName() == PreferredSkeletalMeshComponentName)
			{
				return Mesh;
			}
		}
	}

	// 2) 없으면 첫 SkeletalMeshComponent 사용(가장 단순)
	return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

UAnimInstance* UMosesGestureDeckComponent::GetAnimInstanceFromMesh(USkeletalMeshComponent* Mesh) const
{
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

void UMosesGestureDeckComponent::LogDeckOrder(const TCHAR* Prefix) const
{
	FString OrderStr;
	for (int32 i = 0; i < DeckOrder.Num(); ++i)
	{
		OrderStr += FString::Printf(TEXT("%d%s"), DeckOrder[i], (i == DeckOrder.Num() - 1) ? TEXT("") : TEXT(","));
	}

	UE_LOG(LogTemp, Log, TEXT("%s Order=[%s] Cursor=%d LastIndex=%d Pool=%d"),
		Prefix, *OrderStr, Cursor, LastIndex, GesturePool.Num());
}
