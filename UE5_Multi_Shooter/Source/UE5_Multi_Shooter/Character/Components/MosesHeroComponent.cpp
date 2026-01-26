#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

// ============================================================================
// Ctor / Lifecycle
// ============================================================================

UMosesHeroComponent::UMosesHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 전용
	if (!IsLocalPlayerPawn())
	{
		return;
	}
}

// ============================================================================
// Local guard
// ============================================================================

bool UMosesHeroComponent::IsLocalPlayerPawn() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	return PC && PC->IsLocalController();
}

// ============================================================================
// Camera delegate
// ============================================================================

TSubclassOf<UMosesCameraMode> UMosesHeroComponent::DetermineCameraMode() const
{
	// 우선순위: Sniper2x > FPS > TPS
	if (bWantsSniper2x && Sniper2xModeClass)
	{
		return Sniper2xModeClass;
	}

	if (bWantsFirstPerson && FirstPersonModeClass)
	{
		return FirstPersonModeClass;
	}

	return ThirdPersonModeClass;
}

void UMosesHeroComponent::TryBindCameraModeDelegate_LocalOnly()
{
	if (!IsLocalPlayerPawn())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(Pawn);
	if (!CamComp)
	{
		UE_LOG(LogMosesCamera, Warning, TEXT("[Hero][CamBind] No MosesCameraComponent Pawn=%s"), *GetNameSafe(Pawn));
		return;
	}

	if (CamComp->DetermineCameraModeDelegate.IsBound())
	{
		return;
	}

	CamComp->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);

	UE_LOG(LogMosesCamera, Log,
		TEXT("[Hero][CamBind] Bind DetermineCameraModeDelegate OK Pawn=%s PC=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(Pawn->GetController()));
}

// ============================================================================
// Input binding entry
// ============================================================================

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

	// 최소 필수: 이동/스프린트/점프/상호작용
	if (!InputMappingContext || !IA_Move || !IA_Sprint || !IA_Jump || !IA_Interact)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero][Input] Assets missing. Set IMC + IA_Move/Sprint/Jump/Interact."));
		return;
	}

	AddMappingContextOnce();
	BindInputActions(PlayerInputComponent);
}

// ============================================================================
// Mapping context
// ============================================================================

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

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero][Input] AddMappingContext OK Pawn=%s Priority=%d"),
		*GetNameSafe(Pawn), MappingPriority);
}

// ============================================================================
// Bind actions
// ============================================================================

void UMosesHeroComponent::BindInputActions(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero][Input] Expected EnhancedInputComponent."));
		return;
	}

	// Move / Look
	EIC->BindAction(IA_Move.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleMove);

	if (IA_Look)
	{
		EIC->BindAction(IA_Look.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleLook);
	}

	// Jump
	EIC->BindAction(IA_Jump.Get(), ETriggerEvent::Started, this, &ThisClass::HandleJump);

	// Sprint (hold)
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSprintPressed);
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);

	// Interact
	EIC->BindAction(IA_Interact.Get(), ETriggerEvent::Started, this, &ThisClass::HandleInteract);

	// Camera (optional)
	if (IA_ToggleView)
	{
		EIC->BindAction(IA_ToggleView.Get(), ETriggerEvent::Started, this, &ThisClass::HandleToggleView);
	}

	if (IA_Aim)
	{
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Started, this, &ThisClass::HandleAimPressed);
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleAimReleased);
	}

	// [ADD] Equip slots (optional but DAY3 핵심 검증)
	if (IA_EquipSlot1)
	{
		EIC->BindAction(IA_EquipSlot1.Get(), ETriggerEvent::Started, this, &ThisClass::HandleEquipSlot1);
	}
	if (IA_EquipSlot2)
	{
		EIC->BindAction(IA_EquipSlot2.Get(), ETriggerEvent::Started, this, &ThisClass::HandleEquipSlot2);
	}
	if (IA_EquipSlot3)
	{
		EIC->BindAction(IA_EquipSlot3.Get(), ETriggerEvent::Started, this, &ThisClass::HandleEquipSlot3);
	}

	// [ADD] Fire (DAY3 핵심)
	if (IA_Fire)
	{
		EIC->BindAction(IA_Fire.Get(), ETriggerEvent::Started, this, &ThisClass::HandleFirePressed);
	}

	bInputBound = true;
	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero][Input] Bind OK Pawn=%s"), *GetNameSafe(GetOwner()));
}

// ============================================================================
// Handlers -> PlayerCharacter endpoints
// ============================================================================

void UMosesHeroComponent::HandleMove(const FInputActionValue& Value)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	PlayerChar->Input_Move(Value.Get<FVector2D>());
}

void UMosesHeroComponent::HandleLook(const FInputActionValue& Value)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	PlayerChar->Input_Look(Value.Get<FVector2D>());
}

void UMosesHeroComponent::HandleJump(const FInputActionValue& Value)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	PlayerChar->Input_JumpPressed();
}

void UMosesHeroComponent::HandleSprintPressed(const FInputActionValue& Value)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	PlayerChar->Input_SprintPressed();
}

void UMosesHeroComponent::HandleSprintReleased(const FInputActionValue& Value)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	PlayerChar->Input_SprintReleased();
}

void UMosesHeroComponent::HandleInteract(const FInputActionValue& Value)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerChar)
	{
		return;
	}

	PlayerChar->Input_InteractPressed();
}

// [ADD] Equip / Fire handlers
void UMosesHeroComponent::HandleEquipSlot1(const FInputActionValue& Value)
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner()))
	{
		PlayerChar->Input_EquipSlot1();
	}
}

void UMosesHeroComponent::HandleEquipSlot2(const FInputActionValue& Value)
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner()))
	{
		PlayerChar->Input_EquipSlot2();
	}
}

void UMosesHeroComponent::HandleEquipSlot3(const FInputActionValue& Value)
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner()))
	{
		PlayerChar->Input_EquipSlot3();
	}
}

void UMosesHeroComponent::HandleFirePressed(const FInputActionValue& Value)
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner()))
	{
		PlayerChar->Input_FirePressed();
	}
}

// ============================================================================
// Camera state handlers
// ============================================================================

void UMosesHeroComponent::HandleToggleView(const FInputActionValue& Value)
{
	bWantsFirstPerson = !bWantsFirstPerson;

	// 정책: FPS 해제 시 Sniper도 해제
	if (!bWantsFirstPerson)
	{
		bWantsSniper2x = false;
	}

	UE_LOG(LogMosesCamera, Log, TEXT("[Hero][Cam] ToggleView -> FirstPerson=%d"), bWantsFirstPerson ? 1 : 0);
}

void UMosesHeroComponent::HandleAimPressed(const FInputActionValue& Value)
{
	if (Sniper2xModeClass)
	{
		bWantsSniper2x = true;
		UE_LOG(LogMosesCamera, Log, TEXT("[Hero][Cam] AimPressed -> Sniper2x ON"));
	}
}

void UMosesHeroComponent::HandleAimReleased(const FInputActionValue& Value)
{
	bWantsSniper2x = false;
	UE_LOG(LogMosesCamera, Log, TEXT("[Hero][Cam] AimReleased -> Sniper2x OFF"));
}
