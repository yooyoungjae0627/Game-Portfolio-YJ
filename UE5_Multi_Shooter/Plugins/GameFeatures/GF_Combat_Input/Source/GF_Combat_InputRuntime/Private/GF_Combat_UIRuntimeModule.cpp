// Copyright ...

#include "GF_Combat_InputRuntimeModule.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FGF_Combat_InputRuntimeModule, GF_Combat_InputRuntime)

void FGF_Combat_InputRuntimeModule::StartupModule()
{
	// 개발자 주석:
	// - 모듈 로드 시 1회 호출.
	// - 필요하면 여기서 GameFeature 입력 관련 로그/등록 진입점을 추가한다.
}

void FGF_Combat_InputRuntimeModule::ShutdownModule()
{
	// 개발자 주석:
	// - 모듈 언로드 시 호출.
}
