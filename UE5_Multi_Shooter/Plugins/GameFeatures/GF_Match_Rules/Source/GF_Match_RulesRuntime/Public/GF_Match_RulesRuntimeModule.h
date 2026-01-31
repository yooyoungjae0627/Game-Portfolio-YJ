// ============================================================================
// GF_Match_RulesRuntimeModule.h (FULL)
// ----------------------------------------------------------------------------
// - GameFeature 플러그인의 런타임 모듈 엔트리.
// - GameFeatureAction이 핵심 동작을 담당하므로 모듈은 Side Effect를 최소화한다.
// ============================================================================

#pragma once

#include "Modules/ModuleManager.h"

class FGF_Match_RulesRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
