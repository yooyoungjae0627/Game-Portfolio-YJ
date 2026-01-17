#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "UE5_Multi_Shooter/Input/MosesInputComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "TimerManager.h" // [MOD]
#include "Engine/World.h" // [MOD]
#include "InputMappingContext.h" 

UMosesHeroComponent::UMosesHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = Cast<APawn>(GetOwner());
	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] BeginPlay Pawn=%s Local=%d"),
		*GetNameSafe(Pawn),
		IsLocalPlayerPawn() ? 1 : 0);

	if (!IsLocalPlayerPawn())
	{
		return;
	}

	// Camera delegate (기존 유지)
	if (UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(Pawn))
	{
		if (!CamComp->DetermineCameraModeDelegate.IsBound())
		{
			CamComp->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
			UE_LOG(LogMosesSpawn, Warning, TEXT("[Hero] Bind DetermineCameraModeDelegate OK Pawn=%s"), *GetNameSafe(Pawn));
		}
	}

	// BeginPlay 타이밍에서 InputComponent가 아직 준비 안 된 경우가 많아서
	// "즉시 1회 시도 + 실패 시 짧게 재시도"로 안정화한다. // [MOD]
	TryInitializePlayerInput(); // [MOD]
}

bool UMosesHeroComponent::IsLocalPlayerPawn() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	const AController* Controller = Pawn->GetController();
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC && PC->IsLocalController();
}

TSubclassOf<UMosesCameraMode> UMosesHeroComponent::DetermineCameraMode() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}

	const UMosesPawnExtensionComponent* PawnExt = UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (!PawnExt)
	{
		return nullptr;
	}

	const UMosesPawnData* PawnData = PawnExt->GetPawnData<UMosesPawnData>();
	if (!PawnData)
	{
		return nullptr;
	}

	return PawnData->DefaultCameraMode;
}

void UMosesHeroComponent::TryInitializePlayerInput() // [MOD]
{
	// 이미 바인딩 끝났으면 더 이상 할 필요 없음
	if (bInputBound)
	{
		return;
	}

	// 로컬 아니면 하지 않음
	if (!IsLocalPlayerPawn())
	{
		return;
	}

	InitializePlayerInput();

	// InitializePlayerInput 안에서 bInputBound가 true가 되지 않았다면 아직 준비가 안 된 것
	if (!bInputBound)
	{
		ScheduleRetryInitializePlayerInput();
	}
}

void UMosesHeroComponent::ScheduleRetryInitializePlayerInput() // [MOD]
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이미 타이머가 돌고 있으면 중복 스케줄 금지
	if (World->GetTimerManager().IsTimerActive(InputInitRetryTimerHandle))
	{
		return;
	}

	// 너무 오래 재시도는 하지 않음 (로그/안정성)
	if (InputInitAttempts >= InputInitMaxAttempts)
	{
		APawn* Pawn = Cast<APawn>(GetOwner());
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero] Input init retry exceeded. Pawn=%s PC=%s (Check SetupPlayerInputComponent / InputClass Settings)"),
			*GetNameSafe(Pawn), *GetNameSafe(PC));
		return;
	}

	// 다음 프레임/짧은 딜레이 후 다시 시도
	World->GetTimerManager().SetTimer(
		InputInitRetryTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::TryInitializePlayerInput),
		InputInitRetryIntervalSec,
		false);

	++InputInitAttempts;

	UE_LOG(LogMosesSpawn, Verbose, TEXT("[Hero] ScheduleRetryInitializePlayerInput attempt=%d/%d"),
		InputInitAttempts, InputInitMaxAttempts);
}

void UMosesHeroComponent::InitializePlayerInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] InitializePlayerInput Pawn=%s PC=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(PC));

	if (!Pawn || !PC)
	{
		return;
	}

	// 입력 에셋 누락 체크
	if (!InputMappingContext || !IA_Move || !IA_Sprint || !IA_Jump || !IA_Interact)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero] Input assets missing. Set IMC + IA_* in BP. Pawn=%s"), *GetNameSafe(Pawn));
		return;
	}

	// --- 1) MappingContext 추가 (1회) ---
	if (!bMappingContextAdded) // [MOD]
	{
		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (!LP)
		{
			return;
		}

		UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (!Subsystem)
		{
			return;
		}

		Subsystem->AddMappingContext(InputMappingContext.Get(), MappingPriority);
		bMappingContextAdded = true; // [MOD]

		UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] AddMappingContext OK IMC=%s Priority=%d Pawn=%s"),
			*GetNameSafe(InputMappingContext.Get()),
			MappingPriority,
			*GetNameSafe(Pawn));
	}

	// --- 2) InputComponent 준비 여부 확인 ---
	// BeginPlay 직후엔 PC->InputComponent가 아직 원하는 타입이 아닐 수 있다. // [MOD]
	UMosesInputComponent* MosesIC = Cast<UMosesInputComponent>(PC->InputComponent);
	if (!MosesIC)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[Hero] InputComponent not ready (not UMosesInputComponent yet). Will retry. PC=%s CurrentIC=%s"),
			*GetNameSafe(PC), *GetNameSafe(PC->InputComponent)); // [MOD]
		return; // [MOD] -> TryInitializePlayerInput에서 재시도
	}

	// 이미 바인딩 했으면 중복 금지
	if (bInputBound) // [MOD]
	{
		return;
	}

	// --- 3) Bind Actions ---
	MosesIC->BindAction(IA_Move.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
	MosesIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSprintPressed);
	MosesIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);
	MosesIC->BindAction(IA_Jump.Get(), ETriggerEvent::Started, this, &ThisClass::HandleJumpPressed);
	MosesIC->BindAction(IA_Interact.Get(), ETriggerEvent::Started, this, &ThisClass::HandleInteractPressed);

	bInputBound = true; // [MOD]

	// 재시도 타이머가 돌고 있으면 정리
	if (UWorld* World = GetWorld()) // [MOD]
	{
		World->GetTimerManager().ClearTimer(InputInitRetryTimerHandle);
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] EnhancedInput Bind OK Pawn=%s"), *GetNameSafe(Pawn));
}

void UMosesHeroComponent::HandleMove(const FInputActionValue& Value)
{
	AMosesCharacter* Character = Cast<AMosesCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	const FVector2D MoveValue = Value.Get<FVector2D>();
	Character->Input_Move(MoveValue);
}

void UMosesHeroComponent::HandleSprintPressed(const FInputActionValue& Value)
{
	AMosesCharacter* Character = Cast<AMosesCharacter>(GetOwner());
	if (Character)
	{
		Character->Input_SprintPressed();
	}
}

void UMosesHeroComponent::HandleSprintReleased(const FInputActionValue& Value)
{
	AMosesCharacter* Character = Cast<AMosesCharacter>(GetOwner());
	if (Character)
	{
		Character->Input_SprintReleased();
	}
}

void UMosesHeroComponent::HandleJumpPressed(const FInputActionValue& Value)
{
	AMosesCharacter* Character = Cast<AMosesCharacter>(GetOwner());
	if (Character)
	{
		Character->Input_JumpPressed();
	}
}

void UMosesHeroComponent::HandleInteractPressed(const FInputActionValue& Value)
{
	AMosesCharacter* Character = Cast<AMosesCharacter>(GetOwner());
	if (Character)
	{
		Character->Input_InteractPressed();
	}
}
