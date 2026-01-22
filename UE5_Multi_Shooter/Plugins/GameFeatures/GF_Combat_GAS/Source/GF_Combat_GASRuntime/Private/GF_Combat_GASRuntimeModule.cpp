#include "GF_Combat_GASRuntimeModule.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FGF_Combat_GASRuntimeModule, GF_Combat_GASRuntime)

void FGF_Combat_GASRuntimeModule::StartupModule()
{
	// 개발자 주석:
	// - 모듈이 로드될 때 한 번 호출된다.
	// - 여기서 싱글톤/서브시스템 등록 등을 할 수 있다.
}

void FGF_Combat_GASRuntimeModule::ShutdownModule()
{
	// 개발자 주석:
	// - 모듈 언로드 시 호출된다.
}
