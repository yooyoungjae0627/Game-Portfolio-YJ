// Copyright ...

#pragma once

#include "Modules/ModuleManager.h"

/**
 * GF_Combat_InputRuntime 모듈
 *
 * 개발자 주석:
 * - GameFeature(Input) 쪽 C++ 확장을 위한 런타임 모듈 엔트리 포인트.
 * - Day2 단계에서는 "빌드/로드 가능"을 1차 목표로 두고,
 *   이후 GameFeatureAction(입력 데이터 등록/적용)을 이 모듈에서 확장한다.
 */
class FGF_Combat_InputRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
