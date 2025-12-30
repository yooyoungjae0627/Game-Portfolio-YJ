// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/GameFrameworkInitStateInterface.h"   
#include "Components/PawnComponent.h"  
#include "MosesHeroComponent.generated.h"

class UMosesCameraMode;
template<class TClass> class TSubclassOf;

// EnhancedInput 관련 구조체들(다른 헤더에 정의돼 있음)
//struct FYJMappableConfigPair;
//struct FInputActionValue;
//class UInputComponent;

/**
 * UMosesHeroComponent
 *
 * - 플레이어가 조종하는 Pawn(캐릭터)의 "입력 + 카메라" 시스템을 초기화하는 컴포넌트
 * - GameFrameworkComponentManager 의 InitState 체인에 참여하여
 *   PawnExtensionComponent / PawnData / PlayerState / Controller 등이
 *   전부 준비된 이후에만 실제 입력 바인딩과 카메라 모드 설정을 수행한다.
 *
 * - 붙는 위치:
 *   보통 캐릭터 BP(AYJCharacter 기반 BP)에 Add Component 해서 사용.
 */

UCLASS(Blueprintable, Meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesHeroComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
	
public:
	// 기본 생성자
	UMosesHeroComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** GameFrameworkComponentManager 에 등록할 Feature 이름 ("Hero" Feature) */
	static const FName NAME_ActorFeatureName;

	/** GameFeatureAction_AddInputConfig 에서 사용할 확장 이벤트 이름 ("BindInputsNow") */
	static const FName NAME_BindInputsNow;

	// ─────────────────────────────────────────────
	// UPawnComponent interface
	// ─────────────────────────────────────────────
	virtual void OnRegister() final;                                  // 컴포넌트가 Actor에 등록될 때 호출
	virtual void BeginPlay() final;                                   // 게임 시작 시
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) final; // 게임 끝/파괴 시

	// ─────────────────────────────────────────────
	// IGameFrameworkInitStateInterface 구현부
	// ─────────────────────────────────────────────
	virtual FName GetFeatureName() const final { return NAME_ActorFeatureName; }

	// 다른 Feature(예: PawnExtension)의 InitState 가 바뀔 때 호출되는 콜백
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) final;

	// 현재 상태 → 목표 상태로 넘어갈 수 있는지 검사 (조건 체크)
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager,
		FGameplayTag CurrentState, FGameplayTag DesiredState) const final;

	// InitState 가 실제로 바뀔 때(특히 DataAvailable → DataInitialized) 해야 할 작업 수행
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager,
		FGameplayTag CurrentState, FGameplayTag DesiredState) final;

	// Spawned → DataAvailable → DataInitialized → GameplayReady 체인을 주어진 순서대로 진행
	virtual void CheckDefaultInitialization() final;

	// ─────────────────────────────────────────────
	// 멤버 함수들
	// ─────────────────────────────────────────────

	/**
	 * 현재 Pawn / PawnData 를 기반으로 사용할 카메라 모드 클래스를 결정
	 * - PawnExtensionComponent → PawnData → DefaultCameraMode 를 꺼내는 역할
	 */
	TSubclassOf<UMosesCameraMode> DetermineCameraMode() const;

	/**
	 * 실제로 EnhancedInput 시스템에 입력을 바인딩하는 함수
	 * - PlayerInputComponent 는 엔진이 생성해주는 입력 컴포넌트(UInputComponent 기반)
	 * - 내부에서 UYJInputComponent 로 캐스팅해서 사용
	 */
	void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	///** 이동 액션 콜백 (InputTag_Move / IA_Move 로 들어오는 입력 처리) */
	//void Input_Move(const FInputActionValue& InputActionValue);

	///** 마우스 Look 액션 콜백 (InputTag_Look_Mouse / IA_Look_Mouse 로 들어오는 입력 처리) */
	//void Input_LookMouse(const FInputActionValue& InputActionValue);

	/** Ability 입력 - 키 눌림 (InputTag 기반) */
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);

	/** Ability 입력 - 키 뗌 (InputTag 기반) */
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

public:
	/**
	 * 기본으로 활성화할 PlayerMappableInputConfig 목록
	 *
	 * - 각 요소(FYJMappableConfigPair)는
	 *   TSoftObjectPtr<UPlayerMappableInputConfig> + bShouldActivateAutomatically 를 가진다.
	 * - Game 시작 시 / Pawn 준비 완료 시점에
	 *   EnhancedInputLocalPlayerSubsystem 에 등록되어 MappingContext 들을 깔 수 있다.
	 */
	//UPROPERTY(EditAnywhere, Category = "YJ|Input")
	//TArray<FYJMappableConfigPair> DefaultInputConfigs;
	
};
