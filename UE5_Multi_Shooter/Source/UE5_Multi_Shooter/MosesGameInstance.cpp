// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesGameInstance.h"

#include "Components/GameFrameworkComponentManager.h"
#include "UE5_Multi_Shooter/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"
#include "Engine/Engine.h"

UMosesGameInstance* UMosesGameInstance::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	// 가장 정석: WorldContext로부터 GI 가져오기
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return Cast<UMosesGameInstance>(World->GetGameInstance());
	}

	// 혹시 GetWorld가 안 되는 컨텍스트면(드물게) Kismet로 한 번 더 시도
	if (UWorld* World2 = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return Cast<UMosesGameInstance>(World2->GetGameInstance());
	}

	return nullptr;
}

UMosesGameInstance* UMosesGameInstance::GetChecked(const UObject* WorldContextObject)
{
	UMosesGameInstance* GI = Get(WorldContextObject);
	if (!GI)
	{
		UE_LOG(LogMosesAuth, Error, TEXT("[GI] UMosesGameInstance::GetChecked failed. WorldContextObject=%s"),
			*GetNameSafe(WorldContextObject));
	}
	return GI;
}

void UMosesGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogMosesExp, Log, TEXT("[GI] Init"));

	// ✅ World가 아직 없을 수 있으니, World 생성 이후 훅에서 찍는다.
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UMosesGameInstance::TryLogServerBoot);

	// GameInstance 시작 시 실행되는 초기화 구간
	// 여기서 앞서 정의한 GameplayTags(InitState들)를
	// GameFrameworkComponentManager에 등록해 상태 흐름을 구성한다.

	// GameFrameworkComponentManager 가져오기 (Actor/Component 초기화 상태 관리용 매니저)
	UGameFrameworkComponentManager* ComponentManager = GetSubsystem<UGameFrameworkComponentManager>(this);
	if (ensure(ComponentManager)) // 없으면 크래시 방지 + 로그 발생
	{
		// 네이티브 GameplayTags 싱글톤에서 InitState 태그들 가져오기
		const FMosesGameplayTags& GameplayTags = FMosesGameplayTags::Get();

		/**
		 * RegisterInitState(Tag, bOptional, RequiredBeforeTag)
		 *
		 * - Tag                : 등록할 초기화 상태 태그
		 * - bOptional          : true = 선택적 상태 / false = 필수 상태
		 * - RequiredBeforeTag  : 어떤 태그 이전에 수행해야 하는지 (상태 선행 조건)
		 *
		 * 여기서는 단계가 아래처럼 연결된다(상태머신 구축):
		 *
		 * InitState.Spawned → InitState.DataAvailable → InitState.DataInitialized → InitState.GameplayReady
		 */

		 // [1단계] 스폰됨(Spawned) 상태 등록 (최초 상태, 선행 필요 없음)
		ComponentManager->RegisterInitState(GameplayTags.InitState_Spawned, false, FGameplayTag());

		// [2단계] 데이터 사용 가능(DataAvailable) → Spawned 이후 진행됨
		ComponentManager->RegisterInitState(GameplayTags.InitState_DataAvailable, false, GameplayTags.InitState_Spawned);

		// [3단계] 데이터 내부 초기화 완료(DataInitialized) → DataAvailable 이후 진행됨
		ComponentManager->RegisterInitState(GameplayTags.InitState_DataInitialized, false, GameplayTags.InitState_DataAvailable);

		// [4단계] 실제 게임 플레이 준비(GameplayReady) → DataInitialized 이후 최종 완료 상태
		ComponentManager->RegisterInitState(GameplayTags.InitState_GameplayReady, false, GameplayTags.InitState_DataInitialized);
	}
}

void UMosesGameInstance::Shutdown()
{
	Super::Shutdown();
	// Game 종료 시 호출
	// 특별한 정리는 필요 없으므로 현재는 기본 동작만 수행
}

void UMosesGameInstance::TryLogServerBoot(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (bDidServerBootLog || !World)
		return;

	// ✅ GameWorld만 (에디터/프리뷰 월드 제외)
	if (!World->IsGameWorld())
		return;

	// ✅ 서버쪽만
	if (World->GetNetMode() == NM_Client)
		return;

	bDidServerBootLog = true;

	UE_LOG(LogMosesAuth, Log, TEXT("[AUTH] Server Boot (NetMode=%d, World=%s)"),
		(int32)World->GetNetMode(), *World->GetName());

	// ✅ 한 번 찍었으면 델리게이트 제거(불필요 호출 방지)
	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
}



