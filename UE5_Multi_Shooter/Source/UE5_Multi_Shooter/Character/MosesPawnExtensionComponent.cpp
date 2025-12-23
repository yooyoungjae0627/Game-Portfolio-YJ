// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesPawnExtensionComponent.h"

#include "Components/GameFrameworkComponentManager.h"   
#include "UE5_Multi_Shooter/MosesGameplayTags.h"               
#include "UE5_Multi_Shooter/MosesLogChannels.h"               

const FName UMosesPawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

UMosesPawnExtensionComponent::UMosesPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 기본적으로 Tick 사용 안 함
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesPawnExtensionComponent::SetPawnData(const UMosesPawnData* InPawnData)
{
	// Pawn 은 반드시 있어야 한다.
	APawn* Pawn = GetPawnChecked<APawn>();

	// PawnData 설정은 서버 권한에서만 허용
	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// 이미 PawnData 가 세팅되어 있다면 다시 바꾸지 않음
	if (PawnData)
	{
		return;
	}

	// 최초 1회만 PawnData 세팅
	PawnData = InPawnData;
}

void UMosesPawnExtensionComponent::SetupPlayerInputComponent()
{
	// 캐릭터의 SetupPlayerInputComponent() 에서 호출되는 진입점
	// 여기서는 InitState 체인(Spawned → DataAvailable → ...) 을 다시 한 번 확인하여
	// 아직 올라가지 않은 상태가 있으면 올려준다.
	//CheckDefaultInitialization();
}

void UMosesPawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	// 올바른 Actor(=Pawn)에 붙어 있는지 확인
	if (GetPawn<APawn>() == nullptr)
	{
		UE_LOG(LogMoses, Error, TEXT("this component has been added to a BP whose base class is not a Pawn!"));
		return;
	}

	// GameFrameworkComponentManager 에 이 컴포넌트를 Feature 로 등록
	RegisterInitStateFeature();

	// 이 Actor 에 대한 Manager 를 가져오지만,
	// 지금은 딱히 쓰지는 않고 있다(필요하면 나중에 사용 가능).
	UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(GetOwningActor());
}

void UMosesPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	//// 다른 Feature 들의 InitState 가 변경될 때 알림을 받기 위해 바인딩
	//// NAME_None : 모든 Feature 이름에 대해 콜백 받겠다는 의미
	//BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	//// 처음 InitState 를 Spawned 로 올려보려고 시도
	//TryToChangeInitState(FYJGameplayTags::Get().InitState_Spawned);


	//// Spawned → DataAvailable → DataInitialized → GameplayReady 순서로
	//// 갈 수 있는지 검사하고, 조건 되면 올려주는 함수
	//CheckDefaultInitialization();
}

void UMosesPawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// GameFrameworkComponentManager 에서 Feature 등록 해제
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UMosesPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	// 내 FeatureName 이 아닌 다른 Feature 의 InitState 변경에만 반응
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();

		// 어떤 Feature 가 DataAvailable 상태가 되었다면,
		// 나도 그에 맞춰 다음 상태로 갈 수 있는지 다시 체크
		if (Params.FeatureState == InitTags.InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}

bool UMosesPawnExtensionComponent::CanChangeInitState(
	UGameFrameworkComponentManager* Manager,
	FGameplayTag CurrentState,
	FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();
	const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();

	// ─────────────────────────────
	// (없음) → Spawned
	// ─────────────────────────────
	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		// Pawn 만 존재하면 Spawned 로 올라갈 수 있다.
		if (Pawn)
		{
			return true;
		}
	}

	// ─────────────────────────────
	// Spawned → DataAvailable
	// ─────────────────────────────
	if (CurrentState == InitTags.InitState_Spawned &&
		DesiredState == InitTags.InitState_DataAvailable)
	{
		// PawnData 가 세팅되지 않았다면 DataAvailable 로 못감
		if (!PawnData)
		{
			return false;
		}

		// 로컬로 컨트롤되는 Pawn 이라면 Controller 도 붙어 있어야 함
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		if (bIsLocallyControlled)
		{
			if (!GetController<AController>())
			{
				return false;
			}
		}

		return true;
	}

	// ─────────────────────────────
	// DataAvailable → DataInitialized
	// ─────────────────────────────
	if (CurrentState == InitTags.InitState_DataAvailable &&
		DesiredState == InitTags.InitState_DataInitialized)
	{
		// 모든 Feature가 최소한 DataAvailable 상태가 되었을 때
		// 비로소 내 상태를 DataInitialized 로 올릴 수 있다.
		return Manager->HaveAllFeaturesReachedInitState(Pawn, InitTags.InitState_DataAvailable);
	}

	// ─────────────────────────────
	// DataInitialized → GameplayReady
	// ─────────────────────────────
	if (CurrentState == InitTags.InitState_DataInitialized &&
		DesiredState == InitTags.InitState_GameplayReady)
	{
		// 추가 조건 없이 true
		return true;
	}

	return false;
}

void UMosesPawnExtensionComponent::CheckDefaultInitialization()
{
	// 부모 클래스(혹시 상속받은 다른 구현)가 있다면 그쪽도 InitState 체크
	CheckDefaultInitializationForImplementers();

	const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();

	// 이 컴포넌트가 따라야 하는 InitState 체인 정의
	static const TArray<FGameplayTag> StateChain =
	{
		InitTags.InitState_Spawned,
		InitTags.InitState_DataAvailable,
		InitTags.InitState_DataInitialized,
		InitTags.InitState_GameplayReady
	};

	// 위에서 정의한 순서대로,
	// CanChangeInitState 가 true 를 반환하는 한 계속 상태를 올려준다.
	ContinueInitStateChain(StateChain);
}



