#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "UE5_Multi_Shooter/MosesPlayerController.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

UMosesHeroComponent::UMosesHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerPawn())
	{
		return;
	}
}

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

TSubclassOf<UMosesCameraMode> UMosesHeroComponent::DetermineCameraMode() const
{
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

	const bool bHasLookSplit = (IA_LookYaw != nullptr) || (IA_LookPitch != nullptr);

	// [MOD][RELOAD] IA_Reload 추가는 "필수 에셋"으로 강제하지 않음(로비/모드별로 비울 수 있으니)
	// 다만, 전투 맵에서 리로드가 필요하면 에셋이 세팅되어 있어야 함.

	if (!InputMappingContext || !IA_Move || !IA_Sprint || !IA_Jump || !IA_Interact || !bHasLookSplit)
	{
		UE_LOG(LogMosesSpawn, Error,
			TEXT("[Hero][Input] Assets missing. Need IMC + IA_Move/Sprint/Jump/Interact + (IA_Look OR IA_LookYaw/Pitch)."));
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

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero][Input] AddMappingContext OK Pawn=%s Priority=%d"),
		*GetNameSafe(Pawn), MappingPriority);
}

void UMosesHeroComponent::BindInputActions(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero][Input] Expected EnhancedInputComponent."));
		return;
	}

	EIC->BindAction(IA_Move.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleMove);

	if (IA_LookYaw)
	{
		EIC->BindAction(IA_LookYaw.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleLookYaw);
	}

	if (IA_LookPitch)
	{
		EIC->BindAction(IA_LookPitch.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleLookPitch);
	}

	EIC->BindAction(IA_Jump.Get(), ETriggerEvent::Started, this, &ThisClass::HandleJump);

	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSprintPressed);
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);

	EIC->BindAction(IA_Interact.Get(), ETriggerEvent::Started, this, &ThisClass::HandleInteract);

	if (IA_ToggleView)
	{
		EIC->BindAction(IA_ToggleView.Get(), ETriggerEvent::Started, this, &ThisClass::HandleToggleView);
	}

	if (IA_Aim)
	{
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Started, this, &ThisClass::HandleAimPressed);
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleAimReleased);
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Canceled, this, &ThisClass::HandleAimReleased);
	}

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
	if (IA_EquipSlot4) // [MOD][SLOT4]
	{
		EIC->BindAction(IA_EquipSlot4.Get(), ETriggerEvent::Started, this, &ThisClass::HandleEquipSlot4);
	}

	if (IA_Reload) // [MOD][RELOAD]
	{
		EIC->BindAction(IA_Reload.Get(), ETriggerEvent::Started, this, &ThisClass::HandleReload);
	}

	if (IA_Fire)
	{
		EIC->BindAction(IA_Fire.Get(), ETriggerEvent::Started, this, &ThisClass::HandleFirePressed);
		EIC->BindAction(IA_Fire.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleFireReleased);
		EIC->BindAction(IA_Fire.Get(), ETriggerEvent::Canceled, this, &ThisClass::HandleFireReleased);
	}

	bInputBound = true;
	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero][Input] Bind OK Pawn=%s"), *GetNameSafe(GetOwner()));
}

// ---- Forward to Pawn ----

void UMosesHeroComponent::HandleMove(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_Move(Value.Get<FVector2D>());
	}
}

void UMosesHeroComponent::HandleLookYaw(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		const float RawYaw = Value.Get<float>();
		const float Yaw = RawYaw * LookSensitivityYaw;

		PC->Input_LookYaw(Yaw);
	}
}

void UMosesHeroComponent::HandleLookPitch(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		float RawPitch = Value.Get<float>();
		float Pitch = RawPitch * LookSensitivityPitch;

		if (bInvertPitch)
		{
			Pitch *= -1.0f;
		}

		PC->Input_LookPitch(Pitch);
	}
}

void UMosesHeroComponent::HandleJump(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_JumpPressed();
	}
}

void UMosesHeroComponent::HandleSprintPressed(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_SprintPressed();
	}
}

void UMosesHeroComponent::HandleSprintReleased(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_SprintReleased();
	}
}

void UMosesHeroComponent::HandleInteract(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_InteractPressed();
	}
}

void UMosesHeroComponent::HandleEquipSlot1(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_EquipSlot1();
	}
}

void UMosesHeroComponent::HandleEquipSlot2(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_EquipSlot2();
	}
}

void UMosesHeroComponent::HandleEquipSlot3(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_EquipSlot3();
	}
}

void UMosesHeroComponent::HandleEquipSlot4(const FInputActionValue& Value) // [MOD][SLOT4]
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_EquipSlot4();
	}
}

void UMosesHeroComponent::HandleReload(const FInputActionValue& Value) // [MOD][RELOAD]
{
	(void)Value;

	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[RELOAD][CL][Hero] Forward -> Pawn::Input_Reload Pawn=%s"), *GetNameSafe(PC));
		PC->Input_Reload();
	}
}

void UMosesHeroComponent::HandleFirePressed(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_FirePressed();
	}
}

void UMosesHeroComponent::HandleFireReleased(const FInputActionValue& Value)
{
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetOwner()))
	{
		PC->Input_FireReleased();
	}
}

// ---- Camera toggles ----

void UMosesHeroComponent::HandleToggleView(const FInputActionValue& Value)
{
	bWantsFirstPerson = !bWantsFirstPerson;

	if (!bWantsFirstPerson)
	{
		bWantsSniper2x = false;
	}

	UE_LOG(LogMosesCamera, Log, TEXT("[Hero][Cam] ToggleView -> FirstPerson=%d"), bWantsFirstPerson ? 1 : 0);
}

void UMosesHeroComponent::HandleAimPressed(const FInputActionValue& /*Value*/)
{
	if (!IsLocalPlayerPawn())
	{
		return;
	}

	if (bWantsSniper2x)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(PC);
	if (!MosesPC)
	{
		return;
	}

	MosesPC->Scope_OnPressed_Local();

	if (!MosesPC->IsScopeActive_Local())
	{
		UE_LOG(LogMosesCamera, Warning, TEXT("[Hero][Cam] AimPressed REJECT (ScopeNotActive) Pawn=%s"), *GetNameSafe(Pawn));
		return;
	}

	bWantsSniper2x = true;
	UE_LOG(LogMosesCamera, Warning, TEXT("[Hero][Cam] AimPressed -> Sniper2x ON Pawn=%s"), *GetNameSafe(Pawn));
}

void UMosesHeroComponent::HandleAimReleased(const FInputActionValue& /*Value*/)
{
	if (!IsLocalPlayerPawn())
	{
		return;
	}

	if (!bWantsSniper2x)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (AMosesPlayerController* MosesPC = Cast<AMosesPlayerController>(PC))
	{
		MosesPC->Scope_OnReleased_Local();
	}

	bWantsSniper2x = false;
	UE_LOG(LogMosesCamera, Warning, TEXT("[Hero][Cam] AimReleased -> Sniper2x OFF Pawn=%s"), *GetNameSafe(Pawn));
}
