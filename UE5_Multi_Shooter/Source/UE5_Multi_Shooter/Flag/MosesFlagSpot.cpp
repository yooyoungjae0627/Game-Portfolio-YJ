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
	// ---------------------------------------------------------------------
	// 네트워크 정책
	// - 오버랩 자체는 로컬 판정이지만, InteractionTarget을 서버로 전달하는 흐름에서
	//   Spot Actor 참조가 안정적으로 전달되도록 Replicate ON 권장.
	// - Spot은 레벨 고정(이동 없음), 개수도 6개 수준이라 비용은 매우 작다.
	// ---------------------------------------------------------------------
	bReplicates = true;
	SetReplicateMovement(false);
	NetDormancy = DORM_DormantAll;
	SetNetUpdateFrequency(1.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpotMesh"));
	SpotMesh->SetupAttachment(Root);
	SpotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ---------------------------------------------------------------------
	// CaptureZone: 오버랩 기반(조준/TraceTarget 제거)
	// - Pawn만 Overlap
	// ---------------------------------------------------------------------
	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(Root);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(true);
	CaptureZone->InitSphereRadius(260.f);

	// ---------------------------------------------------------------------
	// PromptWidget: 로컬 플레이어만 보이는 안내(기본 Hidden)
	// ---------------------------------------------------------------------
	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(Root);
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetTwoSided(true);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComponent->SetVisibility(false);
	PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
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

	// 시스템이 꺼지면 진행 중 캡처는 서버에서 취소 확정
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

	if (!bFlagSystemEnabled)
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT(SystemDisabled) Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	if (!CanStartCapture_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT Guard Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	if (IsDead_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT Dead Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	// 이미 다른 플레이어가 캡처 중이면 거절
	if (CapturerPS && CapturerPS != RequesterPS)
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT AlreadyCapturing Spot=%s Capturer=%s Requester=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(this), *GetNameSafe(CapturerPS), *GetNameSafe(RequesterPS));
		return false;
	}

	// 이미 본인이 캡처 중이면 OK
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

	// SSOT 갱신(캡처 시작)
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

	// 사망 시 성공 불가(서버 최종 판정)
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

	// 서버 방송(Announcement)
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

	// ---------------------------------------------------------------------
	// 클라 로컬: 존에 들어오면 프롬프트 표시 + InteractTarget SET
	// - 조준/TraceTarget 없이 "존 안"이면 타겟이 된다.
	// ---------------------------------------------------------------------
	if (!HasAuthority())
	{
		if (IsLocalPawn(Pawn))
		{
			LocalPromptPawn = Pawn;

			ApplyPromptText_Local();
			SetPromptVisible_Local(true);

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
	// 클라 로컬: 존 이탈이면 프롬프트 숨김 + InteractTarget CLEAR
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
	// 서버: 캡처 중인 플레이어가 존을 나가면 즉시 취소
	// ---------------------------------------------------------------------
	AMosesPlayerState* PS = Pawn->GetPlayerState<AMosesPlayerState>();
	if (PS && CapturerPS == PS)
	{
		CancelCapture_Internal(EMosesCaptureCancelReason::LeftZone);
	}
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

	const FText InteractText = FText::FromString(TEXT("E : 캡처"));
	const FText NameText = FText::FromString(TEXT("FLAG"));

	Prompt->SetPromptTexts(InteractText, NameText);
}

bool AMosesFlagSpot::IsLocalPawn(const APawn* Pawn) const
{
	return (Pawn && Pawn->IsLocallyControlled());
}

UMosesInteractionComponent* AMosesFlagSpot::GetInteractionComponentFromPawn(APawn* Pawn) const
{
	return Pawn ? Pawn->FindComponentByClass<UMosesInteractionComponent>() : nullptr;
}

bool AMosesFlagSpot::CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const
{
	// 확장 지점: Phase/Warmup/Match 제한 등을 추가 가능
	return (RequesterPS != nullptr);
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
	// 데이터 에셋이 있으면 그 값을 우선 사용
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

	// 폴백
	return 4.0f;
}
