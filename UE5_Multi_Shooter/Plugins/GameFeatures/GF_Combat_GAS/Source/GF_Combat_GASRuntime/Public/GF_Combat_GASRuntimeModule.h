#pragma once

#include "Modules/ModuleManager.h"

/**
 * 개발자 주석:
 * - GF_Combat_GASRuntime 모듈의 최소 엔트리 포인트.
 * - 지금 단계(Day2)에서는 “컴파일/로드 가능” 상태만 보장해도 충분.
 * - 이후 GameFeatureAction(AbilitySet 등록 등)을 C++로 확장할 때 이 모듈을 기반으로 작업.
 */
class FGF_Combat_GASRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
