#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "MosesDeveloperCheatManager.generated.h"

/**
 * UMosesDeveloperCheatManager
 *
 * [목적]
 * - 스모크 테스트를 콘솔(~)에서 즉시 실행한다.
 *
 * [콘솔]
 * - Moses_AssetValidate : Validate + (서버면) 무기 스폰/부착
 * - Moses_Hit           : HitReact 몽타주
 * - Moses_Death         : Death 몽타주
 * - Moses_Attack        : Attack 몽타주(좀비)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesDeveloperCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec) void Moses_AssetValidate();
	UFUNCTION(Exec) void Moses_Hit();
	UFUNCTION(Exec) void Moses_Death();
	UFUNCTION(Exec) void Moses_Attack();

private:
	class UMosesAssetBootstrapComponent* FindAssetBootstrapComponentOnPawn() const;
};
