#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesPawnSSOTGuardComponent.generated.h"

/**
 * MosesPawnSSOTGuardComponent
 *
 * [목표]
 * - Pawn(Body)에는 SSOT 데이터(HP/Ammo/Score 등)가 "존재하면 안 된다".
 * - 런타임에 Pawn 클래스의 모든 UPROPERTY 이름을 스캔하여,
 *   금지 키워드가 있으면 즉시 Error 로그 + ensure로 잡아낸다.
 *
 * [왜 필요한가?]
 * - “절대 두지 않는다”는 규칙은 사람 실수로 깨진다.
 * - 이 컴포넌트를 Pawn에 붙이면, 규칙 위반을 자동으로 감지한다.
 */

UCLASS(ClassGroup=(Moses), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesPawnSSOTGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesPawnSSOTGuardComponent();

protected:
	virtual void BeginPlay() override;

private:
	void ValidateOwnerClassProperties() const;
	bool IsForbiddenPropertyName(const FString& PropertyName) const;
};
