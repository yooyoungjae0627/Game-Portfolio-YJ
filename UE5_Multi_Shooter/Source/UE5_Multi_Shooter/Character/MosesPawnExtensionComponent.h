// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/PawnComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "MosesPawnExtensionComponent.generated.h"

class UMosesPawnData;

/**
 * UYJPawnExtensionComponent
 *
 * - Pawn(캐릭터)의 “확장 정보”를 담당하는 컴포넌트
 *   (이 프로젝트에서는 PawnData, InitState 관리의 중심 역할)
 *
 * - 하는 일:
 *   1) 이 Pawn이 어떤 PawnData(UYJPawnData)를 쓸지 들고 있음
 *      (무기 세트, 카메라, 입력 설정, 움직임 설정 등)
 *   2) InitState (Spawned, DataAvailable, DataInitialized, GameplayReady) 체인을 통해
 *      "이 Pawn이 지금 어느 정도까지 준비되었는지"를 GameFrameworkComponentManager 에 보고
 *   3) 다른 Feature(HeroComponent, CameraComponent, AbilitySystemComponent 등)가
 *      "PawnExtension이 DataInitialized까지 왔는지"를 보고 자기 초기화를 진행할 수 있게 해준다.
 */

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesPawnExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
	
public:
	UMosesPawnExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** GameFrameworkComponentManager 에 등록할 Feature 이름 ("PawnExtension") */
	static const FName NAME_ActorFeatureName;

	// ─────────────────────────────────────────────
	// 헬퍼 / 유틸 함수
	// ─────────────────────────────────────────────

	/** Pawn(또는 Actor) 에서 UYJPawnExtensionComponent 를 찾아주는 정적 함수 */
	static UMosesPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UMosesPawnExtensionComponent>() : nullptr);
	}

	/**
	 * PawnData 를 원하는 타입으로 꺼내는 템플릿 함수
	 *
	 * 사용 예:
	 *   const UYJPawnData* Data = PawnExt->GetPawnData<UYJPawnData>();
	 */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/**
	 * PawnData 를 세팅하는 함수
	 *
	 * - 서버 권한(Authority) 에서만 세팅 가능
	 * - 한번 세팅된 뒤에는 다시 설정되지 않도록 방어 로직 포함
	 */
	void SetPawnData(const UMosesPawnData* InPawnData);

	/**
	 * 캐릭터의 SetupPlayerInputComponent() 에서 호출해주는 진입점
	 *
	 * - 내부적으로 InitState 체인을 검사/진행(CheckDefaultInitialization)해서
	 *   "입력을 받을 준비가 되었는지" 검사하고, 필요한 경우 다음 상태로 올리게 된다.
	 * - 실제 입력 바인딩은 HeroComponent::InitializePlayerInput 에서 수행.
	 */
	void SetupPlayerInputComponent();

	// ─────────────────────────────────────────────
	// UPawnComponent interface
	// ─────────────────────────────────────────────
	virtual void OnRegister() final;                                  // 컴포넌트가 Actor에 Attach될 때
	virtual void BeginPlay() final;                                   // 게임 시작 시
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) final; // 종료 시

	// ─────────────────────────────────────────────
	// IGameFrameworkInitStateInterface 구현부
	// ─────────────────────────────────────────────

	/** 이 컴포넌트의 Feature 이름을 반환 (PawnExtension) */
	virtual FName GetFeatureName() const final { return NAME_ActorFeatureName; }

	/**
	 * 다른 Feature(또는 자기 자신)의 InitState 가 바뀔 때 호출
	 * - 여기서는 "다른 Feature가 DataAvailable 이 되었는지" 관찰하고
	 *   그에 따라 자기 InitState 를 더 올릴 수 있는지 체크한다.
	 */
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) final;

	/**
	 * InitState 전환 가능 여부를 확인하는 함수
	 *
	 * 예:
	 *  - (없음) → Spawned : Pawn 이 실제로 존재하는지
	 *  - Spawned → DataAvailable : PawnData, Controller(로컬) 등 준비되었는지
	 *  - DataAvailable → DataInitialized : 전체 Feature 가 최소 DataAvailable 에 도달했는지
	 *  - DataInitialized → GameplayReady : 추가 조건 없이 true
	 */
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager,
		FGameplayTag CurrentState, FGameplayTag DesiredState) const final;

	/**
	 * 기본 InitState 체인(Spawned → DataAvailable → DataInitialized → GameplayReady)을
	 * 순서대로 체크하면서 가능한 만큼 올려주는 함수
	 */
	virtual void CheckDefaultInitialization() final;

public:
	/**
	 * 이 Pawn 이 사용할 각종 설정이 들어있는 DataAsset
	 *
	 * - 예를 들어:
	 *   - DefaultCameraMode
	 *   - InputConfig
	 *   - AbilitySet
	 *   - MovementSettings
	 *   등등이 UYJPawnData 안에 정의되어 있을 것으로 예상.
	 *
	 * - EditInstanceOnly:
	 *   각 레벨에 배치된 Pawn(캐릭터)마다 다른 PawnData 를 줄 수 있다.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Moses|Pawn")
	TObjectPtr<const UMosesPawnData> PawnData;
	
	
};
