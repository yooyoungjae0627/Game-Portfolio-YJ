// ============================================================================
// MosesCharacter.cpp
// ============================================================================

#include "UE5_Multi_Shooter/Character/MosesCharacter.h"

#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

#include "UE5_Multi_Shooter/Pickup/PickupBase.h"

AMosesCharacter::AMosesCharacter()
{
	bReplicates = true;

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PawnExtComponent = CreateDefaultSubobject<UMosesPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));

	CameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetRootComponent());
	CameraComponent->SetRelativeLocation(FVector(-300.f, 0.f, 75.f));

	bUseControllerRotationYaw = true;
}

bool AMosesCharacter::IsSprinting() const
{
	// [FIX] 로컬은 예측 값으로 UI/Anim을 더 부드럽게, 원격은 복제값
	if (IsLocallyControlled())
	{
		return bLocalPredictedSprinting;
	}

	return bIsSprinting;
}

// =========================================================
// Engine
// =========================================================

void AMosesCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 초기 속도 적용(서버/클라 공통)
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}

	UE_LOG(LogMosesMove, Log, TEXT("[MOVE] BeginPlay WalkSpeed=%.0f SprintSpeed=%.0f Pawn=%s Role=%s"),
		WalkSpeed, SprintSpeed, *GetNameSafe(this), *UEnum::GetValueAsString(GetLocalRole()));
}

void AMosesCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLateJoinInitTimer();
	Super::EndPlay(EndPlayReason);
}

void AMosesCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMosesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 입력 바인딩은 PC->InputComponent가 아니라, 엔진이 준비된 InputComponent로 확정한다.
	if (HeroComponent)
	{
		HeroComponent->SetupInputBindings(PlayerInputComponent);
	}
	else
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[Hero] Missing HeroComponent Pawn=%s"), *GetNameSafe(this));
	}
}

void AMosesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	UE_LOG(LogMosesGAS, Log, TEXT("[GAS][SV] PossessedBy Pawn=%s Controller=%s"),
		*GetNameSafe(this), *GetNameSafe(NewController));

	TryInitASC_FromPlayerState();

	// [FIX] 서버는 SSOT 값을 기반으로 속도 동기화
	ApplySprintSpeed_FromAuthoritativeState(TEXT("PossessedBy"));
}

void AMosesCharacter::UnPossessed()
{
	StopLateJoinInitTimer();
	Super::UnPossessed();
}

void AMosesCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	APlayerState* RawPS = GetPlayerState();
	UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][CL] OnRep_PlayerState Pawn=%s RawPS=%s"),
		*GetNameSafe(this), *GetNameSafe(RawPS));

	if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(RawPS))
	{
		PS->TryInitASC(this);
		StopLateJoinInitTimer();
		return;
	}

	StartLateJoinInitTimer();
}

void AMosesCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesCharacter, bIsSprinting);
}

// =========================================================
// GAS Init
// =========================================================

void AMosesCharacter::TryInitASC_FromPlayerState()
{
	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		StartLateJoinInitTimer();
		return;
	}

	PS->TryInitASC(this);
	StopLateJoinInitTimer();
}

void AMosesCharacter::StartLateJoinInitTimer()
{
	if (TimerHandle_LateJoinInit.IsValid())
	{
		return;
	}

	LateJoinRetryCount = 0;

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_LateJoinInit,
		this,
		&ThisClass::LateJoinInitTick,
		LateJoinRetryIntervalSec,
		true);

	UE_LOG(LogMosesGAS, Verbose, TEXT("[GAS] Start LateJoinInitTimer Pawn=%s"), *GetNameSafe(this));
}

void AMosesCharacter::StopLateJoinInitTimer()
{
	if (TimerHandle_LateJoinInit.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_LateJoinInit);
	}
}

void AMosesCharacter::LateJoinInitTick()
{
	++LateJoinRetryCount;

	AMosesPlayerState* PS = GetPlayerState<AMosesPlayerState>();
	if (PS)
	{
		UE_LOG(LogMosesGAS, Log, TEXT("[GAS] LateJoinInit SUCCESS Pawn=%s Retry=%d"),
			*GetNameSafe(this), LateJoinRetryCount);

		PS->TryInitASC(this);
		StopLateJoinInitTimer();
		return;
	}

	if (LateJoinRetryCount >= MaxLateJoinRetries)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS] LateJoinInit FAIL Pawn=%s Retries=%d"),
			*GetNameSafe(this), LateJoinRetryCount);

		StopLateJoinInitTimer();
	}
}

// =========================================================
// Input
// =========================================================

void AMosesCharacter::Input_Move(const FVector2D& MoveValue)
{
	if (Controller && (MoveValue.X != 0.0f || MoveValue.Y != 0.0f))
	{
		const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

		AddMovementInput(Forward, MoveValue.Y);
		AddMovementInput(Right, MoveValue.X);
	}
}

void AMosesCharacter::Input_SprintPressed()
{
	UE_LOG(LogMosesMove, Warning, TEXT("[Sprint][CL] Input Pressed Pawn=%s"), *GetNameSafe(this));

	// [FIX] 로컬 체감은 로컬 변수로만(Rep 변수 직접 수정 금지)
	ApplySprintSpeed_LocalPredict(true);

	// 서버 확정
	Server_SetSprinting(true);
}

void AMosesCharacter::Input_SprintReleased()
{
	UE_LOG(LogMosesMove, Warning, TEXT("[Sprint][CL] Input Released Pawn=%s"), *GetNameSafe(this));

	ApplySprintSpeed_LocalPredict(false);
	Server_SetSprinting(false);
}

void AMosesCharacter::Input_JumpPressed()
{
	const bool bInAir = GetCharacterMovement() ? GetCharacterMovement()->IsFalling() : false;
	UE_LOG(LogMosesMove, Log, TEXT("[MOVE] Jump Pressed Pawn=%s InAir=%d"), *GetNameSafe(this), bInAir ? 1 : 0);

	Jump();
}

void AMosesCharacter::Input_InteractPressed()
{
	APickupBase* Target = FindPickupTarget_Local();
	UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP] Client Try Target=%s Pawn=%s"),
		*GetNameSafe(Target), *GetNameSafe(this));

	Server_TryPickup(Target);
}

// =========================================================
// Sprint
// =========================================================

void AMosesCharacter::ApplySprintSpeed_LocalPredict(bool bNewSprinting)
{
	// 로컬 플레이어만 예측 허용
	if (!IsLocallyControlled())
	{
		return;
	}

	bLocalPredictedSprinting = bNewSprinting;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bLocalPredictedSprinting ? SprintSpeed : WalkSpeed;
	}

	UE_LOG(LogMosesMove, Log, TEXT("[Sprint][CL] Predict Sprint=%d Speed=%.0f Pawn=%s"),
		bLocalPredictedSprinting ? 1 : 0,
		GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f,
		*GetNameSafe(this));
}

void AMosesCharacter::ApplySprintSpeed_FromAuthoritativeState(const TCHAR* From)
{
	// 서버/OnRep에서 “복제 상태”를 이동속도에 반영
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	// 로컬이면 예측값을 서버 확정값으로 동기화(튐 방지)
	if (IsLocallyControlled())
	{
		bLocalPredictedSprinting = bIsSprinting;
	}

	UE_LOG(LogMosesMove, Warning, TEXT("[Sprint][%s] Apply Auth Sprint=%d Speed=%.0f Pawn=%s Role=%s"),
		From ? From : TEXT("None"),
		bIsSprinting ? 1 : 0,
		GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f,
		*GetNameSafe(this),
		*UEnum::GetValueAsString(GetLocalRole()));
}

void AMosesCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	// ✅ 서버 단일진실
	if (!HasAuthority())
	{
		return;
	}

	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;

	UE_LOG(LogMosesMove, Warning, TEXT("[Sprint][SV] Set Sprint=%d Pawn=%s"),
		bIsSprinting ? 1 : 0,
		*GetNameSafe(this));

	// 서버 즉시 반영(리스닝 포함)
	ApplySprintSpeed_FromAuthoritativeState(TEXT("Server"));
	ForceNetUpdate();
}

void AMosesCharacter::OnRep_IsSprinting()
{
	UE_LOG(LogMosesMove, Warning, TEXT("[Sprint][CL] OnRep Sprint=%d Pawn=%s"),
		bIsSprinting ? 1 : 0,
		*GetNameSafe(this));

	ApplySprintSpeed_FromAuthoritativeState(TEXT("OnRep"));
}

// =========================================================
// Pickup
// =========================================================

APickupBase* AMosesCharacter::FindPickupTarget_Local() const
{
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Forward = GetControlRotation().Vector();
	const FVector End = Start + Forward * PickupTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PickupTrace), false);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return nullptr;
	}

	return Cast<APickupBase>(Hit.GetActor());
}

void AMosesCharacter::Server_TryPickup_Implementation(APickupBase* Target)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=InvalidTarget Pawn=%s"), *GetNameSafe(this));
		return;
	}

	if (Target->IsConsumed())
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=Consumed Target=%s Pawn=%s"),
			*GetNameSafe(Target), *GetNameSafe(this));
		return;
	}

	Target->Server_TryConsume(this);
}
