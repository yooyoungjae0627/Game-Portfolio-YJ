#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "Engine/Engine.h"

// ============================================================
// 기존 DEFINE 
// ============================================================

DEFINE_LOG_CATEGORY(LogMosesSpawn);
DEFINE_LOG_CATEGORY(LogMosesMove);
DEFINE_LOG_CATEGORY(LogMosesPickup);
DEFINE_LOG_CATEGORY(LogMosesGAS);
DEFINE_LOG_CATEGORY(LogMosesAI);

DEFINE_LOG_CATEGORY(LogMosesCombat);
DEFINE_LOG_CATEGORY(LogMosesCamera);

DEFINE_LOG_CATEGORY(LogMosesExp);

DEFINE_LOG_CATEGORY(LogMosesPlayer);
DEFINE_LOG_CATEGORY(LogMosesAuth);
DEFINE_LOG_CATEGORY(LogMosesPhase);
DEFINE_LOG_CATEGORY(LogMosesRoom);

DEFINE_LOG_CATEGORY(LogMosesGF);
DEFINE_LOG_CATEGORY(LogMosesHP);
DEFINE_LOG_CATEGORY(LogMosesGrenade);

DEFINE_LOG_CATEGORY(LogMosesAsset);

// ============================================================
//  세분화 카테고리 DEFINE (선택)
// - h에 선언한 경우, 여기 DEFINE도 반드시 있어야 링크 에러가 안 난다.
// ============================================================

DEFINE_LOG_CATEGORY(LogMosesAmmo);     
DEFINE_LOG_CATEGORY(LogMosesDeath);    
DEFINE_LOG_CATEGORY(LogMosesRespawn);  
DEFINE_LOG_CATEGORY(LogMosesScore);    
DEFINE_LOG_CATEGORY(LogMosesHUD);      

DEFINE_LOG_CATEGORY(LogMosesWeapon);

// ============================================================
// MosesLog Helpers
// ============================================================

namespace MosesLog
{
	const TCHAR* GetWorldNameSafe(const UObject* WorldContext)
	{
		if (WorldContext == nullptr)
		{
			return TEXT("World=NULL");
		}

		const UWorld* World = WorldContext->GetWorld();
		if (World == nullptr)
		{
			return TEXT("World=NULL");
		}

		return *World->GetName();
	}

	const TCHAR* GetNetModeNameSafe(const UObject* WorldContext)
	{
		if (WorldContext == nullptr)
		{
			return TEXT("NetMode=Unknown");
		}

		const UWorld* World = WorldContext->GetWorld();
		if (World == nullptr)
		{
			return TEXT("NetMode=Unknown");
		}

		switch (World->GetNetMode())
		{
		case NM_Standalone:      return TEXT("NetMode=Standalone");
		case NM_DedicatedServer: return TEXT("NetMode=DedicatedServer");
		case NM_ListenServer:    return TEXT("NetMode=ListenServer");
		case NM_Client:          return TEXT("NetMode=Client");
		default:                 break;
		}

		return TEXT("NetMode=Unknown");
	}
}
