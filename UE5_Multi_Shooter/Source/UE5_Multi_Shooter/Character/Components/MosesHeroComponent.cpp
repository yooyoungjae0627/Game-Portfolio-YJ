#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Character/MosesCharacter.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"     // [FIX] UEnhancedInputComponent
#include "InputActionValue.h"
#include "InputMappingContext.h"       // [FIX] UInputMappingContext 정의(로그/PathName)

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

	// Camera delegate
	if (UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(Pawn))
	{
		if (!CamComp->DetermineCameraModeDelegate.IsBound())
		{
			CamComp->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
			UE_LOG(LogMosesSpawn, Warning, TEXT("[Hero] Bind DetermineCameraModeDelegate OK Pawn=%s"), *GetNameSafe(Pawn));
		}
	}

	// 입력 바인딩은 Pawn::SetupPlayerInputComponent에서 수행한다.
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

// =========================================================
// Input (called from Pawn::SetupPlayerInputComponent)
// =========================================================

void UMosesHeroComponent::SetupInputBindings(UInputComponent* PlayerInputComponent)
{
	if (bInputBound)
	{
		return;
	}

	if (!IsLocalPlayerPawn())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] SetupInputBindings Pawn=%s PC=%s IC=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(PC),
		*GetNameSafe(PlayerInputComponent));

	// 입력 에셋 누락 체크
	if (!InputMappingContext || !IA_Move || !IA_Sprint || !IA_Jump || !IA_Interact)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero] Input assets missing. Set IMC + IA_* in BP. Pawn=%s"),
			*GetNameSafe(Pawn));
		return;
	}

	AddMappingContextOnce();
	BindInputActions(PlayerInputComponent);
}

void UMosesHeroComponent::AddMappingContextOnce()
{
	if (bMappingContextAdded)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

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
	bMappingContextAdded = true;

	// [FIX] TObjectPtr 안전 로그
	const UObject* IMCObj = InputMappingContext.Get();
	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] AddMappingContext OK IMC=%s Priority=%d Pawn=%s"),
		IMCObj ? *IMCObj->GetPathName() : TEXT("None"),
		MappingPriority,
		*GetNameSafe(Pawn));
}

void UMosesHeroComponent::BindInputActions(UInputComponent* PlayerInputComponent)
{
	if (bInputBound)
	{
		return;
	}

	// [FIX] UMosesInputComponent 강제 캐스팅 제거.
	//      엔진 표준 EnhancedInputComponent로 바인딩한다.
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent); // [FIX]
	if (!EIC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero] BindInputActions FAILED. Expected UEnhancedInputComponent. Actual=%s Pawn=%s"),
			*GetNameSafe(PlayerInputComponent),
			*GetNameSafe(GetOwner()));
		return;
	}

	EIC->BindAction(IA_Move.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSprintPressed);
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);
	EIC->BindAction(IA_Jump.Get(), ETriggerEvent::Started, this, &ThisClass::HandleJumpPressed);
	EIC->BindAction(IA_Interact.Get(), ETriggerEvent::Started, this, &ThisClass::HandleInteractPressed);

	bInputBound = true;

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] EnhancedInput Bind OK Pawn=%s"), *GetNameSafe(GetOwner()));
}

// =========================================================
// Input Handlers
// =========================================================

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
