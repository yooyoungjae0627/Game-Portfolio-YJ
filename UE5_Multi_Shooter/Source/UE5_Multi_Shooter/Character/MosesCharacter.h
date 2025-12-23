// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Character.h"
#include "MosesCharacter.generated.h"

class UMosesPawnExtensionComponent;
class UMosesCameraComponent;

/**
 * AYJCharacter
 *
 * - 기본 플레이어 캐릭터 클래스
 * - ACharacter 를 상속받아 캡슐, 스켈레탈 메시, 이동 컴포넌트 등을 포함
 * - 여기에 PawnExtensionComponent + CameraComponent 를 붙여서
 *   이 캐릭터가 "데이터 / 카메라 / 입력 시스템"을 사용할 수 있게 해준다.
 *
 * - 입력:
 *   엔진 → SetupPlayerInputComponent() 호출 → PawnExtensionComponent::SetupPlayerInputComponent()
 *   → InitState 진행 → HeroComponent 가 실제 입력 바인딩 수행.
 */

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AMosesCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/**
	 * 엔진이 입력 컴포넌트를 생성해 준 뒤 호출하는 함수
	 *
	 * - 여기서 기본 ACharacter 입력 바인딩이 먼저 수행(Super 호출)
	 * - 이후 PawnExtensionComponent 에게 "입력 준비 해라" 라는 의미로 전달
	 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** 이 Pawn이 사용할 PawnData / InitState를 관리하는 확장 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Character")
	TObjectPtr<UMosesPawnExtensionComponent> PawnExtComponent;

	/** 카메라를 담당하는 커스텀 카메라 컴포넌트 (TPS 카메라 위치 등) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moses|Character")
	TObjectPtr<UMosesCameraComponent> CameraComponent;
	
};
