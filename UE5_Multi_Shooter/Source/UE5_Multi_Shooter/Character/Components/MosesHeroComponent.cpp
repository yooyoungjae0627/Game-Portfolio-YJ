#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"

UMosesHeroComponent::UMosesHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어만 카메라 Delegate 바인딩
	if (!IsLocalPlayerPawn())
	{
		return;
	}

	// 카메라 컴포넌트를 찾아 현재 모드 결정 Delegate를 바인딩한다.
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(Pawn))
		{
			// Delegate가 비어 있으면 바인딩
			if (!CamComp->DetermineCameraModeDelegate.IsBound())
			{
				CamComp->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
				UE_LOG(LogMosesCamera, Log, TEXT("[Hero] Bind DetermineCameraModeDelegate OK Pawn=%s"), *GetNameSafe(Pawn));
			}
		}
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
	// 우선순위: Sniper2x > FPS > TPS
	if (bWantsSniper2x && Sniper2xModeClass)
	{
		return Sniper2xModeClass;
	}

	if (bWantsFirstPerson && FirstPersonModeClass)
	{
		return FirstPersonModeClass;
	}

	// 기본은 TPS
	return ThirdPersonModeClass;
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

	// 최소 입력 에셋 검사
	if (!InputMappingContext || !IA_Move || !IA_Sprint || !IA_Jump || !IA_Interact)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero] Input assets missing. Set IMC + IA_Move/Sprint/Jump/Interact."));
		return;
	}

	AddMappingContextOnce();
	BindInputActions(PlayerInputComponent);
}

void UMosesHeroComponent::TryBindCameraModeDelegate_LocalOnly()
{
	// 로컬 Pawn이 아니면 바인딩하지 않는다.
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

	// 이미 바인딩 되어 있으면 끝
	if (CamComp->DetermineCameraModeDelegate.IsBound())
	{
		return;
	}

	CamComp->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);

	UE_LOG(LogMosesCamera, Warning,
		TEXT("[Hero][CamBind] Bind DetermineCameraModeDelegate OK Pawn=%s PC=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(Pawn->GetController()));
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

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] AddMappingContext OK Pawn=%s"), *GetNameSafe(Pawn));
}

void UMosesHeroComponent::BindInputActions(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[Hero] Expected EnhancedInputComponent."));
		return;
	}

	// 이동/시점
	EIC->BindAction(IA_Move.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
	if (IA_Look)
	{
		EIC->BindAction(IA_Look.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleLook);
	}

	// 점프
	EIC->BindAction(IA_Jump.Get(), ETriggerEvent::Started, this, &ThisClass::HandleJump);

	// 스프린트(누르는 동안)
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSprintPressed);
	EIC->BindAction(IA_Sprint.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);

	// 상호작용
	EIC->BindAction(IA_Interact.Get(), ETriggerEvent::Started, this, &ThisClass::HandleInteract);

	// 카메라 토글(선택)
	if (IA_ToggleView)
	{
		EIC->BindAction(IA_ToggleView.Get(), ETriggerEvent::Started, this, &ThisClass::HandleToggleView);
	}

	// 조준(선택) - 누르는 동안 Sniper2x
	if (IA_Aim)
	{
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Started, this, &ThisClass::HandleAimPressed);
		EIC->BindAction(IA_Aim.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleAimReleased);
	}

	bInputBound = true;
	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] Input bind OK Pawn=%s"), *GetNameSafe(GetOwner()));
}

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

void UMosesHeroComponent::HandleToggleView(const FInputActionValue& Value)
{
	// TPS/FPS 토글
	bWantsFirstPerson = !bWantsFirstPerson;

	// FPS로 바뀌면 Sniper는 즉시 해제(정책)
	if (!bWantsFirstPerson)
	{
		bWantsSniper2x = false;
	}

	UE_LOG(LogMosesCamera, Log, TEXT("[Hero] ToggleView -> FirstPerson=%d"), bWantsFirstPerson ? 1 : 0);
}

void UMosesHeroComponent::HandleAimPressed(const FInputActionValue& Value)
{
	// 조준(저격) On: Sniper 모드가 지정되어 있을 때만
	if (Sniper2xModeClass)
	{
		bWantsSniper2x = true;
		UE_LOG(LogMosesCamera, Log, TEXT("[Hero] AimPressed -> Sniper2x ON"));
	}
}

void UMosesHeroComponent::HandleAimReleased(const FInputActionValue& Value)
{
	bWantsSniper2x = false;
	UE_LOG(LogMosesCamera, Log, TEXT("[Hero] AimReleased -> Sniper2x OFF"));
}
