#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesPawnSSOTGuardComponent.generated.h"

/**
 * UMosesPawnSSOTGuardComponent
 * ============================================================================
 * [목적]
 * - Pawn에 SSOT(정답 상태)가 들어가는 실수를 "에디터 단계"에서 즉시 차단한다.
 * - 특히 LateJoin/Replication 정합성 관점에서, HP/Ammo/Score/Deaths 같은 상태는
 *   PlayerState/CombatComponent로 이동해야 한다.
 *
 * [중요]
 * - Pawn은 입력 엔드포인트/코스메틱 표현을 담당할 수 있다.
 * - 따라서 AnimMontage/Sound/Niagara 같은 "코스메틱 자산 레퍼런스"는 허용한다.
 * - 이 컴포넌트는 "코스메틱 자산"을 SSOT로 오인해 차단하지 않도록 예외 처리가 필수다.
 * ============================================================================
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesPawnSSOTGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesPawnSSOTGuardComponent();

protected:
	virtual void BeginPlay() override;

private:
	/** Pawn에 있어도 허용되는 "코스메틱 자산/표현용 프로퍼티"인지 검사 */
	bool IsCosmeticAllowedProperty(const FProperty* Prop) const;

	/** 이름 기반으로 "SSOT로 오해할만한 상태 프로퍼티"인지 검사 */
	bool IsForbiddenPropertyName(const FString& PropertyName) const;

	/** Owner(Pawn)의 UPROPERTY를 스캔하여 SSOT 위반을 감지 */
	void ValidateOwnerClassProperties() const;
};
