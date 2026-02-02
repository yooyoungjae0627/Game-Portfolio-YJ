// ============================================================================
// MosesFlagSpot.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/PrimitiveComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Flag/MosesFlagFeedbackData.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "UE5_Multi_Shooter/Player/MosesInteractionComponent.h"
#include "UE5_Multi_Shooter/UI/Match/MosesPickupPromptWidget.h"

AMosesFlagSpot::AMosesFlagSpot()
{
	bReplicates = true;
	SetReplicateMovement(false);

	// [MOD] 생성자에서 DormantAll 금지.
	// 초기부터 DormantAll이면 NetGUID/초기 상태 동기 타이밍이 꼬여
	// 상호작용 거리 판정/타겟 일치성에 악영향이 날 수 있다.
	NetDormancy = DORM_Awake; // [MOD] 기존: DORM_DormantAll

	// 빈도는 상황에 따라 조정 가능. (Spot이 정적이면 낮아도 OK)
	SetNetUpdateFrequency(1.0f);

	// ---- Components ----
	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	SetRootComponent(CaptureZone);

	CaptureZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(true);
	CaptureZone->InitSphereRadius(260.f);

	SpotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpotMesh"));
	SpotMesh->SetupAttachment(CaptureZone);
	SpotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(CaptureZone);
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetTwoSided(true);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComponent->SetVisibility(false);
	PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));

	UE_LOG(LogMosesFlag, Warning, TEXT("[FLAGSPOT][CTOR] Root=CaptureZone Radius=%.1f Rep=%d Dormancy=%d"),
		260.f, bReplicates ? 1 : 0, (int32)NetDormancy);
}


void AMosesFlagSpot::BeginPlay()
{
	Super::BeginPlay();

	if (ensure(CaptureZone))
	{
		CaptureZone->OnComponentBeginOverlap.AddDynamic(this, &AMosesFlagSpot::HandleZoneBeginOverlap);
		CaptureZone->OnComponentEndOverlap.AddDynamic(this, &AMosesFlagSpot::HandleZoneEndOverlap);
	}

	SetPromptVisible_Local(false);
	ApplyPromptText_Local();
	StopPromptBillboard_Local();

	UE_LOG(LogMosesFlag, Warning, TEXT("[FLAGSPOT][BeginPlay] Auth=%d NetMode=%d Spot=%s Loc=%s"),
		HasAuthority() ? 1 : 0,
		(GetWorld() ? (int32)GetWorld()->GetNetMode() : -1),
		*GetName(),
		*GetActorLocation().ToString());

	ApplyDormancyPolicy_ServerOnly();
}

void AMosesFlagSpot::ApplyDormancyPolicy_ServerOnly()
{
	if (!HasAuthority())
	{
		return;
	}

	// [MOD] Dormancy를 쓰고 싶다면 "BeginPlay 이후"에 적용.
	// (초기 동기/NetGUID 확정 이후)
	if (bUseDormancyAfterBeginPlay)
	{
		SetNetDormancy(DormancyAfterBeginPlay);

		UE_LOG(LogMosesFlag, Warning, TEXT("[FLAGSPOT][SV][Dormancy] Applied Dormancy=%d Spot=%s"),
			(int32)DormancyAfterBeginPlay, *GetName());
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

	if (!bFlagSystemEnabled && CapturerPS)
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::SystemDisabled);
	}
}

bool AMosesFlagSpot::ServerTryStartCapture(AMosesPlayerState* RequesterPS)
{
	// ---------------------------------------------------------------------
	// 0) Server only
	// ---------------------------------------------------------------------
	if (!HasAuthority())
	{
		return false;
	}

	// ---------------------------------------------------------------------
	// 1) 시스템 비활성화
	// ---------------------------------------------------------------------
	if (!bFlagSystemEnabled)
	{
		UE_LOG(LogMosesFlag, Warning,
			TEXT("%s CaptureStart REJECT(SystemDisabled) Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));

		return false;
	}

	// ---------------------------------------------------------------------
	// 2) 기본 유효성
	// ---------------------------------------------------------------------
	if (!RequesterPS)
	{
		UE_LOG(LogMosesFlag, Warning,
			TEXT("%s CaptureStart REJECT(NullPS) Spot=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this));

		return false;
	}

	// ---------------------------------------------------------------------
	// 3) 캡처 존 안에 있는지 (서버 확정)
	// ⭐ 핵심 보강 포인트
	// ---------------------------------------------------------------------
	if (!IsInsideCaptureZone_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning,
			TEXT("%s CaptureStart REJECT(NotInZone) Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));

		return false;
	}

	// ---------------------------------------------------------------------
	// 4) 캡처 시작 가능 조건
	// ---------------------------------------------------------------------
	if (!CanStartCapture_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning,
			TEXT("%s CaptureStart REJECT(Guard) Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));

		return false;
	}

	// ---------------------------------------------------------------------
	// 5) 사망 상태
	// ---------------------------------------------------------------------
	if (IsDead_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning,
			TEXT("%s CaptureStart REJECT(Dead) Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));

		return false;
	}

	// ---------------------------------------------------------------------
	// 6) 이미 다른 사람이 캡처 중
	// ---------------------------------------------------------------------
	if (CapturerPS && CapturerPS != RequesterPS)
	{
		UE_LOG(LogMosesFlag, Warning,
			TEXT("%s CaptureStart REJECT(AlreadyCapturing) Spot=%s Capturer=%s Requester=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(CapturerPS),
			*GetNameSafe(RequesterPS));

		return false;
	}

	// ---------------------------------------------------------------------
	// 7) 동일 캡처자 재요청 → 허용 (idempotent)
	// ---------------------------------------------------------------------
	if (CapturerPS == RequesterPS)
	{
		return true;
	}

	// ---------------------------------------------------------------------
	// 8) 캡처 시작
	// ---------------------------------------------------------------------
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
			MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(RequesterPS));
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

	UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>();
	if (!CC)
	{
		UE_LOG(LogMosesFlag, Error, TEXT("%s CaptureStart FAIL NoCaptureComponent Player=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(CapturerPS));

		CancelCapture_Internal(EMosesCaptureCancelReason::ServerRejected);
		return;
	}

	CC->ServerSetCapturing(this, true, 0.0f, CaptureHoldSeconds);

	UE_LOG(LogMosesFlag, Log, TEXT("%s CaptureStart OK Spot=%s Player=%s Hold=%.2f"),
		MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(CapturerPS), CaptureHoldSeconds);

	GetWorldTimerManager().ClearTimer(TimerHandle_CaptureTick);
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
			MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(CapturerPS), static_cast<int32>(Reason));
	}

	ResetCaptureState_Server();
}

void AMosesFlagSpot::FinishCapture_Internal()
{
	check(HasAuthority());
	check(CapturerPS);

	GetWorldTimerManager().ClearTimer(TimerHandle_CaptureTick);

	if (IsDead_Server(CapturerPS))
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::Dead);
		return;
	}

	if (UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>())
	{
		CC->ServerOnCaptureSucceeded(this, CaptureElapsedSeconds);
	}

	UE_LOG(LogMosesFlag, Log, TEXT("%s CaptureOK Spot=%s Player=%s Time=%.2f"),
		MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(CapturerPS), CaptureElapsedSeconds);

	if (AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr)
	{
		const FText Text = FText::Format(
			FText::FromString(TEXT("{0} 깃발 캡처 성공!")),
			FText::FromString(GetNameSafe(CapturerPS)));

		const int32 DurationSec = FMath::Max(1, FMath::RoundToInt(ResolveBroadcastSeconds()));
		MGS->ServerStartAnnouncementText(Text, DurationSec);

		UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] FlagCapture Text=\"%s\" Dur=%d"),
			*Text.ToString(), DurationSec);
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

	if (IsDead_Server(CapturerPS))
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::Dead);
		return;
	}

	// ✅ [필수 안정화] 캡처 진행 중에는 "지금도 존 안"인지 계속 확인한다.
	if (CaptureZone)
	{
		const APawn* Pawn = CapturerPS->GetPawn();
		const bool bInZone = (Pawn && CaptureZone->IsOverlappingActor(Pawn));

		if (!IsInsideCaptureZone_Server(CapturerPS))
		{
			CancelCapture_Internal(EMosesCaptureCancelReason::LeftZone);
			return;
		}
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
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	if (!HasAuthority())
	{
		if (IsLocalPawn(Pawn))
		{
			LocalPromptPawn = Pawn;

			ApplyPromptText_Local();
			SetPromptVisible_Local(true);
			StartPromptBillboard_Local();

			if (UMosesInteractionComponent* IC = GetInteractionComponentFromPawn(Pawn))
			{
				IC->SetCurrentInteractTarget_Local(this);
			}

			UE_LOG(LogMosesFlag, Verbose, TEXT("[FLAG][CL] Prompt+Target Show Spot=%s Pawn=%s"),
				*GetNameSafe(this), *GetNameSafe(Pawn));
		}
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
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	// ---------------------------------------------------------------------
	// Client: UI/Target Clear (기존 로직 유지)
	// ---------------------------------------------------------------------
	if (!HasAuthority())
	{
		if (IsLocalPawn(Pawn))
		{
			if (LocalPromptPawn.Get() == Pawn)
			{
				LocalPromptPawn = nullptr;
			}

			SetPromptVisible_Local(false);
			StopPromptBillboard_Local();

			if (UMosesInteractionComponent* IC = GetInteractionComponentFromPawn(Pawn))
			{
				IC->ClearCurrentInteractTarget_Local(this);
			}

			UE_LOG(LogMosesFlag, Verbose, TEXT("[FLAG][CL] Prompt+Target Hide Spot=%s Pawn=%s"),
				*GetNameSafe(this), *GetNameSafe(Pawn));
		}
		return;
	}

	// ---------------------------------------------------------------------
	// Server: No Immediate Cancel (중요)
	// ---------------------------------------------------------------------
	// ✅ EndOverlap 이벤트는 순간 튈 수 있음.
	// ✅ 캡처 유지/취소 판정은 TickCaptureProgress_Server에서 "확정"한다.
	// 따라서 여기서 CancelCapture_Internal을 호출하지 않는다.

	UE_LOG(LogMosesFlag, Verbose, TEXT("%s ZoneExit Spot=%s Pawn=%s (NoImmediateCancel) Capturer=%s"),
		MOSES_TAG_FLAG_SV,
		*GetNameSafe(this),
		*GetNameSafe(Pawn),
		*GetNameSafe(CapturerPS));
}


void AMosesFlagSpot::SetPromptVisible_Local(bool bVisible)
{
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetVisibility(bVisible);
	}
}

void AMosesFlagSpot::ApplyPromptText_Local()
{
	if (!PromptWidgetComponent)
	{
		return;
	}

	UUserWidget* Widget = PromptWidgetComponent->GetUserWidgetObject();
	UMosesPickupPromptWidget* Prompt = Cast<UMosesPickupPromptWidget>(Widget);
	if (!Prompt)
	{
		return;
	}

	Prompt->SetPromptTexts(FText::FromString(TEXT("E : 캡처")), FText::FromString(TEXT("FLAG")));
}

bool AMosesFlagSpot::IsLocalPawn(const APawn* Pawn) const
{
	return (Pawn && Pawn->IsLocallyControlled());
}

UMosesInteractionComponent* AMosesFlagSpot::GetInteractionComponentFromPawn(APawn* Pawn) const
{
	return Pawn ? Pawn->FindComponentByClass<UMosesInteractionComponent>() : nullptr;
}

// ============================================================================
// [MOD] Billboard (Local only)
// ============================================================================

void AMosesFlagSpot::StartPromptBillboard_Local()
{
	if (HasAuthority())
	{
		return;
	}

	if (!PromptWidgetComponent || !PromptWidgetComponent->IsVisible())
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(TimerHandle_PromptBillboard))
	{
		return;
	}

	// 즉시 1회 적용 후 타이머
	ApplyBillboardRotation_Local();

	GetWorldTimerManager().SetTimer(
		TimerHandle_PromptBillboard,
		this,
		&ThisClass::TickPromptBillboard_Local,
		PromptBillboardInterval,
		true);

	UE_LOG(LogMosesFlag, VeryVerbose, TEXT("[FLAG][CL] Billboard START Spot=%s"), *GetNameSafe(this));
}

void AMosesFlagSpot::StopPromptBillboard_Local()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_PromptBillboard))
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_PromptBillboard);
		UE_LOG(LogMosesFlag, VeryVerbose, TEXT("[FLAG][CL] Billboard STOP Spot=%s"), *GetNameSafe(this));
	}
}

void AMosesFlagSpot::TickPromptBillboard_Local()
{
	ApplyBillboardRotation_Local();
}

void AMosesFlagSpot::ApplyBillboardRotation_Local()
{
	if (!PromptWidgetComponent || !PromptWidgetComponent->IsVisible())
	{
		return;
	}

	APawn* Pawn = LocalPromptPawn.Get();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector MyLoc = PromptWidgetComponent->GetComponentLocation();

	FVector ToCam = CamLoc - MyLoc;
	if (ToCam.IsNearlyZero())
	{
		return;
	}

	FRotator LookRot = ToCam.Rotation();

	// ✅ UI는 눕지 않게 Yaw만 쓰는 걸 추천
	LookRot.Pitch = 0.f;
	LookRot.Roll = 0.f;

	PromptWidgetComponent->SetWorldRotation(LookRot);
}

// ============================================================================

bool AMosesFlagSpot::CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const
{
	if (!RequesterPS || !CaptureZone)
	{
		return false;
	}

	const APawn* Pawn = RequesterPS->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	// ✅ 1) 가장 정확: 현재도 존 오버랩 중인지
	if (CaptureZone->IsOverlappingActor(Pawn))
	{
		return true;
	}

	// ✅ 2) 오버랩 이벤트가 순간 흔들릴 때 대비한 거리 백업
	const float Radius = CaptureZone->GetScaledSphereRadius();
	const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), CaptureZone->GetComponentLocation());
	return DistSq <= FMath::Square(Radius);
}

bool AMosesFlagSpot::IsCapturer_Server(const AMosesPlayerState* RequesterPS) const
{
	return (RequesterPS && CapturerPS == RequesterPS);
}

bool AMosesFlagSpot::IsDead_Server(const AMosesPlayerState* PS) const
{
	if (!PS)
	{
		return true;
	}

	const UMosesCombatComponent* Combat = PS->FindComponentByClass<UMosesCombatComponent>();
	return Combat ? Combat->IsDead() : false;
}

float AMosesFlagSpot::ResolveBroadcastSeconds() const
{
	if (FeedbackData.IsValid())
	{
		return FeedbackData.Get()->BroadcastDefaultSeconds;
	}

	if (FeedbackData.ToSoftObjectPath().IsValid())
	{
		if (UMosesFlagFeedbackData* Loaded = FeedbackData.LoadSynchronous())
		{
			return Loaded->BroadcastDefaultSeconds;
		}
	}

	return 4.0f;
}

bool AMosesFlagSpot::IsInsideCaptureZone_Server(const AMosesPlayerState* PS) const
{
	if (!PS || !CaptureZone)
	{
		return false;
	}

	const APawn* Pawn = PS->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	const FVector PawnLoc = Pawn->GetActorLocation();
	const FVector ZoneLoc = CaptureZone->GetComponentLocation();

	// Sphere 기준 서버 판정 (Overlap 이벤트는 힌트일 뿐)
	const float DistSq = FVector::DistSquared(PawnLoc, ZoneLoc);
	const float Radius = CaptureZone->GetScaledSphereRadius();

	const bool bIn = (DistSq <= FMath::Square(Radius));

	UE_LOG(LogMosesFlag, VeryVerbose, TEXT("%s InZone=%d Dist=%.1f Radius=%.1f Pawn=%s Spot=%s"),
		MOSES_TAG_FLAG_SV,
		bIn ? 1 : 0,
		FMath::Sqrt(DistSq),
		Radius,
		*GetNameSafe(Pawn),
		*GetNameSafe(this));

	return bIn;
}
