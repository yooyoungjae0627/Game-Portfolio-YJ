// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesCharacter.h"

#include "MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

// Sets default values
AMosesCharacter::AMosesCharacter()
{
	// 이 캐릭터가 Tick 을 사용할지 여부
	PrimaryActorTick.bCanEverTick = true;
	// 시작 시 Tick 을 켤지 여부 (필요해질 때까지 꺼둔다)
	PrimaryActorTick.bStartWithTickEnabled = false;

	// PawnExtensionComponent 생성
	// - 이 컴포넌트가 PawnData, InitState 체인의 중심이 된다.
	PawnExtComponent = CreateDefaultSubobject<UMosesPawnExtensionComponent>(TEXT("PawnExtensionComponent"));

	// 카메라 컴포넌트 생성
	CameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("CameraComponent"));

	// TPS 관점의 카메라 기본 위치(-300, 0, 75)
	CameraComponent->SetRelativeLocation(FVector(-300.0f, 0.0f, 75.0f));

}

// Called when the game starts or when spawned
void AMosesCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMosesCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMosesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 그 다음 PawnExtensionComponent 에게
	// "입력 준비해라(InitState 진행해라)" 라는 트리거를 던진다.
	// 실제 EnhancedInput 바인딩은 HeroComponent 내부에서 수행된다.
	if (PawnExtComponent)
	{
		PawnExtComponent->SetupPlayerInputComponent();
	}
}

