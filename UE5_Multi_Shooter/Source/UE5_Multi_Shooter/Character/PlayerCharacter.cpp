#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Components/CapsuleComponent.h" 

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"
#include "UE5_Multi_Shooter/Pickup/PickupBase.h"

APlayerCharacter::APlayerCharacter()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	// ViewTarget이 이 Pawn일 때 CameraComponent를 자동 사용하도록
	bFindCameraComponentWhenViewTarget = true;

	// Player 전용 컴포넌트 구성
	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));
	MosesCameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("MosesCameraComponent"));

	if (MosesCameraComponent)
	{
		// [FIX] Root에 붙여 타입/구조 안정성 보장
		MosesCameraComponent->SetupAttachment(GetRootComponent());

		MosesCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		MosesCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		MosesCameraComponent->bAutoActivate = true;
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 기본 걷기 속도 세팅
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// HeroComponent가 EnhancedInput 바인딩을 담당
	if (HeroComponent)
	{
		HeroComponent->SetupInputBindings(PlayerInputComponent);
	}
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 스프린트 상태는 서버가 확정하고 복제
	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
}

void APlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 클라에서 Controller가 “나중에” 붙는 케이스 대응
	if (HeroComponent)
	{
		HeroComponent->TryBindCameraModeDelegate_LocalOnly();
	}
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// ClientRestart/RestartPlayer 타이밍에서 로컬 초기화 재시도
	if (HeroComponent)
	{
		HeroComponent->TryBindCameraModeDelegate_LocalOnly();
	}
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버/리슨 포함: Possess 시점에서도 바인딩 시도 (로컬이면 의미 있음)
	if (HeroComponent)
	{
		HeroComponent->TryBindCameraModeDelegate_LocalOnly();
	}
}

bool APlayerCharacter::IsSprinting() const
{
	// 로컬은 즉시 반응(예측), 원격은 서버 복제값
	return IsLocallyControlled() ? bLocalPredictedSprinting : bIsSprinting;
}

void APlayerCharacter::Input_Move(const FVector2D& MoveValue)
{
	if (!Controller || MoveValue.IsNearlyZero())
	{
		return;
	}

	// 컨트롤 회전(Yaw) 기준으로 이동 방향 계산
	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MoveValue.Y);
	AddMovementInput(Right, MoveValue.X);
}

void APlayerCharacter::Input_Look(const FVector2D& LookValue)
{
	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void APlayerCharacter::Input_SprintPressed()
{
	ApplySprintSpeed_LocalPredict(true);
	Server_SetSprinting(true);
}

void APlayerCharacter::Input_SprintReleased()
{
	ApplySprintSpeed_LocalPredict(false);
	Server_SetSprinting(false);
}

void APlayerCharacter::Input_JumpPressed()
{
	Jump();
}

void APlayerCharacter::Input_InteractPressed()
{
	// 로컬에서 픽업 타겟을 찾고 서버에 요청
	APickupBase* Target = FindPickupTarget_Local();
	Server_TryPickup(Target);
}

void APlayerCharacter::ApplySprintSpeed_LocalPredict(bool bNewSprinting)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	bLocalPredictedSprinting = bNewSprinting;

	// 로컬 즉시 반영(체감)
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bLocalPredictedSprinting ? SprintSpeed : WalkSpeed;
	}
}

void APlayerCharacter::ApplySprintSpeed_FromAuth(const TCHAR* From)
{
	// 서버 확정값 적용
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	// 로컬 예측값도 서버 확정값으로 맞춤
	if (IsLocallyControlled())
	{
		bLocalPredictedSprinting = bIsSprinting;
	}
}

void APlayerCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	// 서버만 상태 변경
	bIsSprinting = bNewSprinting;

	// 서버 자신의 이동속도도 즉시 맞춤
	ApplySprintSpeed_FromAuth(TEXT("Server_SetSprinting"));
}

void APlayerCharacter::OnRep_IsSprinting()
{
	// 클라에서 서버 확정값 반영
	ApplySprintSpeed_FromAuth(TEXT("OnRep_IsSprinting"));
}

APickupBase* APlayerCharacter::FindPickupTarget_Local() const
{
	if (!IsLocallyControlled())
	{
		return nullptr;
	}

	// FollowCamera가 없으므로 MosesCameraComponent 위치를 사용한다.
	const UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(this);
	const FVector Start = CamComp ? CamComp->GetComponentLocation() : GetActorLocation();

	const FRotator Rot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	const FVector End = Start + Rot.Vector() * PickupTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MosesPickupTrace), false, this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return nullptr;
	}

	return Cast<APickupBase>(Hit.GetActor());
}

void APlayerCharacter::Server_TryPickup_Implementation(APickupBase* Target)
{
	if (!Target)
	{
		return;
	}

	// 서버 확정: PickupBase가 원자성/거리 체크 후 확정
	Target->ServerTryConsume(this);
}
