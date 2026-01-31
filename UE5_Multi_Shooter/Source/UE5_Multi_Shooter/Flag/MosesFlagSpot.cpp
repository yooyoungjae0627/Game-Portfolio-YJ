// ============================================================================
// MosesFlagSpot.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Match/MosesBroadcastComponent.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

AMosesFlagSpot::AMosesFlagSpot()
{
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpotMesh"));
	SpotMesh->SetupAttachment(Root);
	SpotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(Root);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(true);
}

void AMosesFlagSpot::BeginPlay()
{
	Super::BeginPlay();

	if (ensure(CaptureZone))
	{
		CaptureZone->OnComponentBeginOverlap.AddDynamic(this, &AMosesFlagSpot::HandleZoneBeginOverlap);
		CaptureZone->OnComponentEndOverlap.AddDynamic(this, &AMosesFlagSpot::HandleZoneEndOverlap);
	}
}

void AMosesFlagSpot::SetFlagSystemEnabled(bool bEnable)
{
	if (!HasAuthority())
	{
		return;
	}

	bFlagSystemEnabled = bEnable;

	UE_LOG(LogMosesFlag, Log, TEXT("%s SystemEnabled=%d Spot=%s"),
		MOSES_TAG_FLAG_SV,
		bFlagSystemEnabled ? 1 : 0,
		*GetNameSafe(this));

	// Disable 시 진행 중이면 즉시 취소
	if (!bFlagSystemEnabled && CapturerPS)
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::SystemDisabled);
	}
}

bool AMosesFlagSpot::ServerTryStartCapture(AMosesPlayerState* RequesterPS)
{
	if (!HasAuthority())
	{
		return false;
	}

	// [UPDATED] 룰 스위치 OFF면 캡처 시작 금지
	if (!bFlagSystemEnabled)
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT(SystemDisabled) Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));
		return false;
	}

	if (!CanStartCapture_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));
		return false;
	}

	if (CapturerPS && CapturerPS != RequesterPS)
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT AlreadyCapturing Spot=%s Capturer=%s Requester=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this), *GetNameSafe(CapturerPS), *GetNameSafe(RequesterPS));
		return false;
	}

	if (CapturerPS == RequesterPS)
	{
		return true;
	}

	StartCapture_Internal(RequesterPS);
	return true;
}

void AMosesFlagSpot::ServerCancelCapture(AMosesPlayerState* RequesterPS, EMosesCaptureCancelReason Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsCapturer_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureCancel IGNORE NotCapturer Spot=%s Requester=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this), *GetNameSafe(RequesterPS));
		return;
	}

	CancelCapture_Internal(Reason);
}

void AMosesFlagSpot::StartCapture_Internal(AMosesPlayerState* NewCapturerPS)
{
	check(HasAuthority());
	check(NewCapturerPS);

	CapturerPS = NewCapturerPS;
	CaptureElapsedSeconds = 0.0f;

	if (UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>())
	{
		CC->ServerSetCapturing(this, true, 0.0f, CaptureHoldSeconds);
	}
	else
	{
		UE_LOG(LogMosesFlag, Error, TEXT("%s CaptureStart FAIL NoCaptureComponent Player=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(CapturerPS));
		CancelCapture_Internal(EMosesCaptureCancelReason::ServerRejected);
		return;
	}

	UE_LOG(LogMosesFlag, Log, TEXT("%s CaptureStart OK Spot=%s Player=%s Hold=%.2f"),
		MOSES_TAG_FLAG_SV,
		*GetNameSafe(this), *GetNameSafe(CapturerPS), CaptureHoldSeconds);

	GetWorldTimerManager().SetTimer(
		TimerHandle_CaptureTick,
		this,
		&AMosesFlagSpot::TickCaptureProgress_Server,
		CaptureTickInterval,
		true);
}

void AMosesFlagSpot::CancelCapture_Internal(EMosesCaptureCancelReason Reason)
{
	check(HasAuthority());

	GetWorldTimerManager().ClearTimer(TimerHandle_CaptureTick);

	if (CapturerPS)
	{
		if (UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>())
		{
			CC->ServerSetCapturing(this, false, 0.0f, CaptureHoldSeconds);
		}

		UE_LOG(LogMosesFlag, Log, TEXT("%s CaptureCancel Spot=%s Player=%s Reason=%d"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this), *GetNameSafe(CapturerPS), static_cast<int32>(Reason));
	}

	ResetCaptureState_Server();
}

void AMosesFlagSpot::FinishCapture_Internal()
{
	check(HasAuthority());
	check(CapturerPS);

	GetWorldTimerManager().ClearTimer(TimerHandle_CaptureTick);

	if (UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>())
	{
		CC->ServerOnCaptureSucceeded(this, CaptureElapsedSeconds);
	}

	UE_LOG(LogMosesFlag, Log, TEXT("%s CaptureOK Spot=%s Player=%s Time=%.2f"),
		MOSES_TAG_FLAG_SV,
		*GetNameSafe(this), *GetNameSafe(CapturerPS), CaptureElapsedSeconds);

	if (AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		if (UMosesBroadcastComponent* BC = GS->FindComponentByClass<UMosesBroadcastComponent>())
		{
			const FString Msg = FString::Printf(TEXT("%s captured the flag!"), *GetNameSafe(CapturerPS));
			BC->ServerBroadcastMessage(Msg, 2.5f);
		}
	}

	ResetCaptureState_Server();
}

void AMosesFlagSpot::TickCaptureProgress_Server()
{
	check(HasAuthority());

	if (!CapturerPS)
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::ServerRejected);
		return;
	}

	// 최소 예시: Dead 판정은 네 SSOT 규칙에 맞춰 연결 가능
	if (CapturerPS->IsOnlyASpectator())
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::Dead);
		return;
	}

	CaptureElapsedSeconds += CaptureTickInterval;

	const float Alpha = FMath::Clamp(CaptureElapsedSeconds / CaptureHoldSeconds, 0.0f, 1.0f);

	if (UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>())
	{
		CC->ServerSetCapturing(this, true, Alpha, CaptureHoldSeconds);
	}

	if (CaptureElapsedSeconds >= CaptureHoldSeconds)
	{
		FinishCapture_Internal();
	}
}

void AMosesFlagSpot::ResetCaptureState_Server()
{
	CapturerPS = nullptr;
	CaptureElapsedSeconds = 0.0f;
}

void AMosesFlagSpot::HandleZoneBeginOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	UE_LOG(LogMosesFlag, Verbose, TEXT("%s ZoneEnter Spot=%s Pawn=%s"),
		MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(Pawn));
}

void AMosesFlagSpot::HandleZoneEndOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	AMosesPlayerState* PS = Pawn->GetPlayerState<AMosesPlayerState>();
	if (PS && CapturerPS == PS)
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::LeftZone);
	}
}

bool AMosesFlagSpot::CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const
{
	return (RequesterPS != nullptr);
}

bool AMosesFlagSpot::IsCapturer_Server(const AMosesPlayerState* RequesterPS) const
{
	return (RequesterPS && CapturerPS == RequesterPS);
}
