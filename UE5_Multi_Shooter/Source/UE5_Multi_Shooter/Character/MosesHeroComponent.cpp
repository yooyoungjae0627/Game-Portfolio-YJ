// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesHeroComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesPawnData.h"                           
#include "MosesPawnExtensionComponent.h"           

#include "EnhancedInputSubsystems.h"           
#include "PlayerMappableInputConfig.h"         

#include "UE5_Multi_Shooter/MosesGameplayTags.h"          
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"

#include "UE5_Multi_Shooter/Character/MosesPawnData.h"

/*#include "UE5_Multi_Shooter/Inputs/YJMappableConfigPair.h"
#include "UE5_Multi_Shooter/Inputs/YJInputComponent.h"    */  

#include "Components/GameFrameworkComponentManager.h"  


// GameFrameworkComponentManager 에 등록할 Feature 이름
const FName UMosesHeroComponent::NAME_ActorFeatureName("Hero");

// InputConfig 의 GameFeatureAction 에서 사용할 Extension Event 이름
const FName UMosesHeroComponent::NAME_BindInputsNow("BindInputsNow");

UMosesHeroComponent::UMosesHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// HeroComponent 는 Tick 을 사용하지 않는다.
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
}

// PawnData 에서 DefaultCameraMode 를 꺼내는 함수
TSubclassOf<UMosesCameraMode> UMosesHeroComponent::DetermineCameraMode() const
{
	// 이 컴포넌트가 붙어 있는 Pawn 얻어오기
	const APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr)
	{
		return nullptr;
	}

	// PawnExtensionComponent 찾기
	if (UMosesPawnExtensionComponent* PawnExtComp = UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		// PawnData 를 UYJPawnData 타입으로 캐스팅하여 가져오기
		if (const UMosesPawnData* PawnData = PawnExtComp->GetPawnData<UMosesPawnData>())
		{
			// PawnData 내에 정의된 기본 카메라 모드 반환
			return PawnData->DefaultCameraMode;
		}
	}

	return nullptr;
}

void UMosesHeroComponent::OnRegister()
{
	Super::OnRegister();

	// HeroComponent 는 반드시 Pawn 기반 Actor 에 붙어야 함
	{
		if (!GetPawn<APawn>())
		{
			//UE_LOG(LogMoses, Error, TEXT("this component has been added to a BP whose base class is not a Pawn!"));
			return;
		}
	}

	// GameFrameworkComponentManager 에 이 Feature 등록 (InitState 에 참여)
	RegisterInitStateFeature();
}

void UMosesHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	//// 필요하다면 여기에서 PawnExtension 의 InitState 변경에 반응하도록 바인딩할 수 있음.
	//BindOnActorInitStateChanged(UYJPawnExtensionComponent::NAME_ActorFeatureName, FGameplayTag(), false);

	//ensure(TryToChangeInitState(FYJGameplayTags::Get().InitState_Spawned));
	CheckDefaultInitialization();
}

void UMosesHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// InitState 시스템에서 Feature 등록 해제
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UMosesHeroComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();

	// PawnExtension Feature 의 상태가 변했을 때만 관심 있음
	//if (Params.FeatureName == UMosesPawnExtensionComponent::NAME_ActorFeatureName)
	//{
	//	// PawnExtension 이 DataInitialized 가 된 순간
	//	if (Params.FeatureState == InitTags.InitState_DataInitialized)
	//	{
	//		// 나도 InitState 체인을 진행시킬 수 있는지 확인
	//		CheckDefaultInitialization();
	//	}
	//}
}

bool UMosesHeroComponent::CanChangeInitState(
	UGameFrameworkComponentManager* Manager,
	FGameplayTag CurrentState,
	FGameplayTag DesiredState) const
{
	check(Manager);

	const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();
	APawn* Pawn = GetPawn<APawn>();
	AMosesPlayerState* MosesPlayerState = GetPlayerState<AMosesPlayerState>();

	// ─────────────────────────────
	// (없음) → Spawned
	// ─────────────────────────────
	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		// Pawn 만 존재하면 Spawned 로 갈 수 있다.
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
		// PlayerState 가 준비되어 있어야 DataAvailable 로 갈 수 있다.
		if (MosesPlayerState  == nullptr)
		{
			return false;
		}

		return true;
	}

	// ─────────────────────────────
	// DataAvailable → DataInitialized
	// ─────────────────────────────
	//if (CurrentState == InitTags.InitState_DataAvailable &&
	//	DesiredState == InitTags.InitState_DataInitialized)
	//{
	//	// PlayerState 가 존재해야 하고,
	//	// PawnExtension Feature 가 이미 DataInitialized 에 도달해 있어야 함.
	//	return MosesPlayerState &&
	//		Manager->HasFeatureReachedInitState(
	//			Pawn,
	//			UMosesPawnExtensionComponent::NAME_ActorFeatureName,
	//			InitTags.InitState_DataInitialized);
	//}

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

void UMosesHeroComponent::HandleChangeInitState(
	UGameFrameworkComponentManager* Manager,
	FGameplayTag CurrentState,
	FGameplayTag DesiredState)
{
	const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();

	// DataAvailable → DataInitialized 로 막 넘어간 순간에만 동작
	if (CurrentState == InitTags.InitState_DataAvailable &&
		DesiredState == InitTags.InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		AMosesPlayerState* MosesPlayerState = GetPlayerState<AMosesPlayerState>();

		// Pawn + PlayerState 는 반드시 있어야 한다.
		if (ensure(Pawn && MosesPlayerState) == false)
		{
			return;
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const UMosesPawnData* PawnData = nullptr;

		// PawnExtensionComponent 찾아서 PawnData 가져오기
		//if (UMosesPawnExtensionComponent* PawnExtComp = UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		//{
		//	PawnData = PawnExtComp->GetPawnData<UMosesPawnData>();

		//	// 이 시점에 AbilitySystem 초기화 등을 할 수 있음 (현재는 주석 처리)
		//	//PawnExtComp->InitializeAbilitySystem(YJPS->GetYJAbilitySystemComponent(), YJPS);
		//}

		// 로컬 플레이어 + PawnData 존재 → 카메라 모드 결정 델리게이트 바인딩
		if (bIsLocallyControlled && PawnData)
		{
			// MosesCharacter 에 붙어 있는 YJCameraComponent 를 찾는다.
			//if (UMosesCameraComponent* CameraComponent = UMosesCameraComponent::FindCameraComponent(Pawn))
			//{
			//	// 카메라가 필요할 때마다 HeroComponent::DetermineCameraMode 를 직접 호출해서
			//	// 어떤 카메라 모드를 쓸지 PawnData 기반으로 결정하게 한다.
			//	CameraComponent->DetermineCameraModeDelegate.BindUObject(
			//		this, &ThisClass::DetermineCameraMode);
			//}
		}

		// 플레이어 컨트롤러가 있고, Pawn 의 InputComponent 가 준비되었다면
		// 실제 입력 바인딩(키/마우스 매핑)을 수행
		if (AMosesPlayerController* MosesPlayerController = GetController<AMosesPlayerController>())
		{
			if (Pawn->InputComponent != nullptr)
			{
				//UE_LOG(LogMoses, Warning, TEXT("PawnInput=%s Class=%s Outer=%s IsCDO=%d"),
		/*			*GetNameSafe(Pawn->InputComponent),
					*GetNameSafe(Pawn->InputComponent ? Pawn->InputComponent->GetClass() : nullptr),
					*GetNameSafe(Pawn->InputComponent ? Pawn->InputComponent->GetOuter() : nullptr),
					Pawn->InputComponent && Pawn->InputComponent->HasAnyFlags(RF_ClassDefaultObject));*/

				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					//UE_LOG(LogMoses, Warning, TEXT("PCInput=%s Class=%s SamePtr=%d"),
		/*				*GetNameSafe(PC->InputComponent),
						*GetNameSafe(PC->InputComponent ? PC->InputComponent->GetClass() : nullptr),
						PC->InputComponent == Pawn->InputComponent);*/
				}

				// EnhancedInput 기반으로 InputContext + 액션 바인딩 세팅
				InitializePlayerInput(Pawn->InputComponent);
			}
		}
	}
}

void UMosesHeroComponent::CheckDefaultInitialization()
{
	// Hero Feature 는 PawnExtension Feature 에 종속되어 있다고 보고,
	// 자신만의 InitState 체인(Spawned → DataAvailable → DataInitialized → GameplayReady)을
	// Manager 에 계속 시도한다.

	const FMosesGameplayTags& InitTags = FMosesGameplayTags::Get();

	static const TArray<FGameplayTag> StateChain =
	{
		InitTags.InitState_Spawned,
		InitTags.InitState_DataAvailable,
		InitTags.InitState_DataInitialized,
		InitTags.InitState_GameplayReady
	};

	// CanChangeInitState 결과에 따라 순서대로 InitState 를 올려줌
	ContinueInitStateChain(StateChain);
}

void UMosesHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	const APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr)
	{
		// Pawn 이 없다면 입력 세팅 불가
		return;
	}

	// LocalPlayer 를 얻기 위해 PlayerController 필요
	const APlayerController* PC = GetController<APlayerController>();
	check(PC);

	// EnhancedInputLocalPlayerSubsystem 을 사용하려면 LocalPlayer 객체가 필요
	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);

	// MappingContext / PlayerMappableInputConfig 를 관리하는 Subsystem
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	// 1) 기존에 등록된 모든 MappingContext 제거
	//    - 캐릭터 교체, 모드 전환 등 상황에서 깔끔하게 초기화하기 위함
	Subsystem->ClearAllMappings();

	// 2) PawnExtensionComponent → PawnData → InputConfig 순으로 접근
	const UMosesPawnExtensionComponent* PawnExtComp =
		UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (PawnExtComp == nullptr)
	{
		return;
	}

	const UMosesPawnData* PawnData = PawnExtComp->GetPawnData<UMosesPawnData>();
	if (PawnData == nullptr)
	{
		return;
	}

	//const UMosesInputConfig* InputConfig = PawnData->InputConfig;
	//if (InputConfig == nullptr)
	//{
	//	return;
	//}

	const FMosesGameplayTags& GameplayTags = FMosesGameplayTags::Get();

	// 2-1) HeroComponent 가 들고 있는 DefaultInputConfigs(PlayerMappableInputConfig 리스트) 순회
	//for (const FYJMappableConfigPair& Pair : DefaultInputConfigs)
	//{
	//	if (Pair.bShouldActivateAutomatically)
	//	{
	//		FModifyContextOptions Options = {};
	//		// 이미 눌려 있는 키를 무시하지 않을지 여부
	//		Options.bIgnoreAllPressedKeysUntilRelease = false;

	//		// 주석 해제하면 PlayerMappableInputConfig 를 실제로 로드 & Subsystem에 등록
	//		//Subsystem->AddPlayerMappableConfig(Pair.Config.LoadSynchronous(), Options);
	//	}
	//}

	//// 2-2) 엔진이 생성한 UInputComponent 를 우리 커스텀 UYJInputComponent 로 캐스팅
	//UMosesInputComponent* MosesInputComponent = CastChecked<UMosesInputComponent>(PlayerInputComponent);
	//{
	//	// 2-2-1) Ability 입력 전체 바인딩 (GameplayTag 기반)
	//	{
	//		TArray<uint32> BindHandles;
	//		YJInputComponent->BindAbilityActions(
	//			InputConfig,
	//			this,
	//			&ThisClass::Input_AbilityInputTagPressed,
	//			&ThisClass::Input_AbilityInputTagReleased,
	//			BindHandles);
	//	}

	//	// 2-2-2) Move 액션 바인딩 (NativeInputActions 중 InputTag_Move)
	//	YJInputComponent->BindNativeAction(
	//		InputConfig,
	//		GameplayTags.InputTag_Move,
	//		ETriggerEvent::Triggered,
	//		this,
	//		&ThisClass::Input_Move,
	//		false);

	//	// 2-2-3) 마우스 Look 액션 바인딩 (InputTag_Look_Mouse)
	//	YJInputComponent->BindNativeAction(
	//		InputConfig,
	//		GameplayTags.InputTag_Look_Mouse,
	//		ETriggerEvent::Triggered,
	//		this,
	//		&ThisClass::Input_LookMouse,
	//		false);
	//}

	// 3) GameFeatureAction_AddInputConfig 같은 곳에서
	//    "이 Pawn 은 이제 입력 바인딩이 끝났다" 라는 확장 이벤트를 받을 수 있도록 신호를 보낸다.
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		const_cast<APawn*>(Pawn),
		NAME_BindInputsNow);
}

//void UMosesHeroComponent::Input_Move(const FInputActionValue& InputActionValue)
//{
//	APawn* Pawn = GetPawn<APawn>();
//	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
//
//	if (Controller)
//	{
//		// EnhancedInput 이 넘겨준 2D 입력 값 (X: 좌/우, Y: 전/후)
//		const FVector2D Value = InputActionValue.Get<FVector2D>();
//
//		// 이동 방향은 카메라의 Yaw 를 기준으로 계산
//		const FRotator MovementRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
//
//		// X 값 → 카메라 기준 오른쪽/왼쪽 이동
//		if (Value.X != 0.0f)
//		{
//			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
//			Pawn->AddMovementInput(MovementDirection, Value.X);
//		}
//
//		// Y 값 → 카메라 기준 앞/뒤 이동
//		if (Value.Y != 0.0f)
//		{
//			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
//			Pawn->AddMovementInput(MovementDirection, Value.Y);
//		}
//	}
//}

//void UMosesHeroComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
//{
//	APawn* Pawn = GetPawn<APawn>();
//	if (!Pawn)
//	{
//		return;
//	}
//
//	const FVector2D Value = InputActionValue.Get<FVector2D>();
//
//	// 마우스 X → Yaw 회전
//	if (Value.X != 0.0f)
//	{
//		Pawn->AddControllerYawInput(Value.X);
//	}
//
//	// 마우스 Y → Pitch 회전 (보통 위아래 반전이므로 -Y)
//	if (Value.Y != 0.0f)
//	{
//		const double AimInversionValue = -Value.Y;
//		Pawn->AddControllerPitchInput(AimInversionValue);
//	}
//}

void UMosesHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		//if (const UMosesPawnExtensionComponent* PawnExtComp = UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		//{
		//	// AbilitySystemComponent 가 있다면 여기서 InputTag 를 전달해 줄 수 있음
		//	//if (UYJAbilitySystemComponent* YJASC = PawnExtComp->GetYJAbilitySystemComponent())
		//	//{
		//	//	YJASC->AbilityInputTagPressed(InputTag);
		//	//}
		//}
	}
}

void UMosesHeroComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		//if (const UMosesPawnExtensionComponent* PawnExtComp = UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		//{
		//	//if (UYJAbilitySystemComponent* YJASC = PawnExtComp->GetYJAbilitySystemComponent())
		//	//{
		//	//	YJASC->AbilityInputTagReleased(InputTag);
		//	//}
		//}
	}
}



