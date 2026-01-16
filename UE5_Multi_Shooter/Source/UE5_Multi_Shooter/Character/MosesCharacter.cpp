#include "UE5_Multi_Shooter/Character/MosesCharacter.h"

#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"

AMosesCharacter::AMosesCharacter()
{
	bReplicates = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PawnExtComponent = CreateDefaultSubobject<UMosesPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	HeroComponent = CreateDefaultSubobject<UMosesHeroComponent>(TEXT("HeroComponent"));

	// ✅ MosesCameraComponent는 UCameraComponent 파생이라 반드시 Root에 붙여야 안정적
	CameraComponent = CreateDefaultSubobject<UMosesCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetRootComponent());
	CameraComponent->SetRelativeLocation(FVector(-300.f, 0.f, 75.f));

	// TPS 기본 회전 정책 (원하면 나중에 조정)
	bUseControllerRotationYaw = true;
}

void AMosesCharacter::BeginPlay()
{
	Super::BeginPlay();
	// GAS Init은 여기서 하지 않음 (PS/포제스 타이밍 보장 X)
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
	// 입력 바인딩은 HeroComponent가 로컬에서 처리(단순 버전)
}

void AMosesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	UE_LOG(LogMosesGAS, Log, TEXT("[GAS][SV] PossessedBy Pawn=%s Controller=%s"),
		*GetNameSafe(this), *GetNameSafe(NewController));

	TryInitASC_FromPlayerState();
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

	// PS가 아직 안 내려온 케이스 방어
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
