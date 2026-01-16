#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "MosesPawnExtensionComponent.generated.h"

class UMosesPawnData;

/**
 * UMosesPawnExtensionComponent (Simple)
 * - PawnData 보관/제공만 담당 (Lyra InitState 체인 제거 버전)
 * - 네트워크/SeamlessTravel/PIE에서 “안 터지는” 안정 루트 우선
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesPawnExtensionComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UMosesPawnExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Actor에서 PawnExt 찾기 */
	static UMosesPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor);

	/** PawnData getter (templated) */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** PawnData가 유효한지 */
	bool HasPawnData() const { return PawnData != nullptr; }

	void SetPawnData(const UMosesPawnData* InPawnData);

protected:
	virtual void BeginPlay() override;

public:
	/**
	 * Pawn이 사용할 DataAsset
	 * - ✅ 권장: BP Defaults(EditDefaultsOnly)로 세팅해서 서버/클라 모두 동일하게 보유
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moses|Pawn")
	TObjectPtr<const UMosesPawnData> PawnData = nullptr;

};
