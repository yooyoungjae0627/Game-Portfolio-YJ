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

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PawnExtComponent = CreateDefaultSubobject<UMosesPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));

	CameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetRootComponent());
	CameraComponent->SetRelativeLocation(FVector(-300.f, 0.f, 75.f));

	bUseControllerRotationYaw = true;
}

void AMosesCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 초기 속도 적용
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}
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

	// [FIX]
	// 입력 바인딩은 "PC->InputComponent"가 아니라,
	// 엔진이 준비된 InputComponent를 넘겨주는 SetupPlayerInputComponent에서 확정한다.
	if (HeroComponent)
	{
		HeroComponent->SetupInputBindings(PlayerInputComponent); // [FIX]
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

	// 서버는 스프린트 상태에 맞춰 속도 강제
	OnRep_IsSprinting();
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
	UE_LOG(LogMosesMove, Log, TEXT("[MOVE] Sprint On Requested Pawn=%s"), *GetNameSafe(this));

	// 로컬 예측(체감)
	ApplySprintSpeed(true);

	// 서버 확정
	Server_SetSprinting(true);
}

void AMosesCharacter::Input_SprintReleased()
{
	UE_LOG(LogMosesMove, Log, TEXT("[MOVE] Sprint Off Requested Pawn=%s"), *GetNameSafe(this));

	ApplySprintSpeed(false);
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

void AMosesCharacter::ApplySprintSpeed(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	UE_LOG(LogMosesMove, Log, TEXT("[MOVE] Sprint %s Speed=%.0f Pawn=%s"),
		bIsSprinting ? TEXT("On") : TEXT("Off"),
		GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f,
		*GetNameSafe(this));
}

void AMosesCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	// ✅ 서버 단일진실
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;
	OnRep_IsSprinting();
}

void AMosesCharacter::OnRep_IsSprinting()
{
	// ✅ 모든 프록시가 동일한 속도 정책을 가지도록 강제
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}
}

APickupBase* AMosesCharacter::FindPickupTarget_Local() const
{
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Forward = GetControlRotation().Vector();
	const FVector End = Start + Forward * PickupTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PickupTrace), /*bTraceComplex*/ false);
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
	// ✅ 서버 검증 1: 권한
	if (!HasAuthority())
	{
		return;
	}

	// ✅ 서버 검증 2: 타겟 유효
	if (!IsValid(Target))
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=InvalidTarget Pawn=%s"), *GetNameSafe(this));
		return;
	}

	// ✅ 서버 검증 3: 이미 소비됨
	if (Target->IsConsumed())
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=Consumed Target=%s Pawn=%s"),
			*GetNameSafe(Target), *GetNameSafe(this));
		return;
	}

	// ✅ 최종 확정: Pickup 자신이 원자성/거리/재진입 방어를 수행
	Target->Server_TryConsume(this);
}

void AMosesCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMosesCharacter, bIsSprinting);
}
