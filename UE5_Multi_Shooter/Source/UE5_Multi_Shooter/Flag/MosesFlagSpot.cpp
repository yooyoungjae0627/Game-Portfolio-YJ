// ============================================================================
// MosesFlagSpot.cpp (FULL)  [MOD]
// ============================================================================

#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Flag/MosesFlagFeedbackData.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "UE5_Multi_Shooter/UI/Match/MosesPickupPromptWidget.h"

AMosesFlagSpot::AMosesFlagSpot()
{
	// 레벨에 배치된 고정 스팟(비복제)로 운용 가능.
	// (서버 권위 판정은 서버에서만 수행되고, 클라는 시각표시만 수행)
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpotMesh"));
	SpotMesh->SetupAttachment(Root);

	// [MOD] LineTrace 타겟은 TraceTarget에서 담당하므로 Mesh 충돌은 꺼도 된다.
	SpotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(Root);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(true);

	// ---------------------------------------------------------------------
	// [MOD] TraceTarget: InteractionComponent(LineTrace ECC_Visibility)을 위한 히트 박스
	// ---------------------------------------------------------------------
	TraceTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("TraceTarget"));
	TraceTarget->SetupAttachment(Root);
	TraceTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TraceTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	TraceTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TraceTarget->SetGenerateOverlapEvents(false);
	TraceTarget->SetBoxExtent(FVector(60.f, 60.f, 80.f));

	// ---------------------------------------------------------------------
	// [MOD] PromptWidget: 월드 말풍선(로컬 UX)
	// ---------------------------------------------------------------------
	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(Root);
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetTwoSided(true);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComponent->SetVisibility(false);

	// 프롬프트 위치 기본값(머리 위)
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

	// [MOD] 프롬프트 기본 Hidden
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

	// 룰 스위치 OFF면 캡처 시작 금지
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
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT Guard Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));
		return false;
	}

	// [MOD] DeadReject (SSOT)
	if (IsDead_Server(RequesterPS))
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT Dead Spot=%s Player=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this),
			*GetNameSafe(RequesterPS));
		return false;
	}

	// 다른 사람이 캡처 중이면 거부(원자성)
	if (CapturerPS && CapturerPS != RequesterPS)
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("%s CaptureStart REJECT AlreadyCapturing Spot=%s Capturer=%s Requester=%s"),
			MOSES_TAG_FLAG_SV,
			*GetNameSafe(this), *GetNameSafe(CapturerPS), *GetNameSafe(RequesterPS));
		return false;
	}

	// 동일 플레이어가 재요청하면 OK로 처리(중복 방지)
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

	UMosesCaptureComponent* CC = CapturerPS->FindComponentByClass<UMosesCaptureComponent>();
	if (!CC)
	{
		UE_LOG(LogMosesFlag, Error, TEXT("%s CaptureStart FAIL NoCaptureComponent Player=%s"),
			MOSES_TAG_FLAG_SV, *GetNameSafe(CapturerPS));
		CancelCapture_Internal(EMosesCaptureCancelReason::ServerRejected);
		return;
	}

	// 진행 시작(0)
	CC->ServerSetCapturing(this, true, 0.0f, CaptureHoldSeconds);

	UE_LOG(LogMosesFlag, Log, TEXT("%s CaptureStart OK Spot=%s Player=%s Hold=%.2f"),
		MOSES_TAG_FLAG_SV,
		*GetNameSafe(this), *GetNameSafe(CapturerPS), CaptureHoldSeconds);

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

	// 최종 검증(Dead 등)
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
		MOSES_TAG_FLAG_SV,
		*GetNameSafe(this), *GetNameSafe(CapturerPS), CaptureElapsedSeconds);

	// ---------------------------------------------------------------------
	// [MOD] 중앙 Announcement는 MatchGameState(RepNotify->Delegate)로 통일
	// ---------------------------------------------------------------------
	if (AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr)
	{
		const FText Text = FText::Format(
			FText::FromString(TEXT("{0} 깃발 캡처 성공!")),
			FText::FromString(GetNameSafe(CapturerPS)));

		const float Sec = ResolveBroadcastSeconds();
		const int32 DurationSec = FMath::Max(1, FMath::RoundToInt(Sec));

		MGS->ServerStartAnnouncementText(Text, DurationSec);

		UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] FlagCapture Text=\"%s\" Dur=%d"),
			*Text.ToString(),
			DurationSec);
	}
	else
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[ANN][SV] FlagCapture FAIL NoMatchGameState"));
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

	// [MOD] DeadReject (SSOT)
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
	// [MOD] 클라: 로컬 UX 프롬프트(서버 권위 아님)
	// ---------------------------------------------------------------------
	if (!HasAuthority())
	{
		if (IsLocalPawn(Pawn))
		{
			LocalPromptPawn = Pawn;
			ApplyPromptText_Local();
			SetPromptVisible_Local(true);

			UE_LOG(LogMosesFlag, Verbose, TEXT("[FLAG][CL] Prompt Show Spot=%s Pawn=%s"),
				*GetNameSafe(this),
				*GetNameSafe(Pawn));
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
	// [MOD] 클라: 로컬 UX 프롬프트 숨김
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

			UE_LOG(LogMosesFlag, Verbose, TEXT("[FLAG][CL] Prompt Hide Spot=%s Pawn=%s"),
				*GetNameSafe(this),
				*GetNameSafe(Pawn));
		}
		return;
	}

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
		// WBP가 UMosesPickupPromptWidget parent가 아닐 수 있으므로 조용히 리턴
		return;
	}

	// 오늘 정책: 플래그 프롬프트는 고정 텍스트(데이터 에셋 불필요)
	const FText InteractText = FText::FromString(TEXT("E : 캡처"));
	const FText NameText = FText::FromString(TEXT("FLAG"));

	Prompt->SetPromptTexts(InteractText, NameText);
}

bool AMosesFlagSpot::IsLocalPawn(const APawn* Pawn) const
{
	return (Pawn && Pawn->IsLocallyControlled());
}

bool AMosesFlagSpot::CanStartCapture_Server(const AMosesPlayerState* RequesterPS) const
{
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
	if (FeedbackData.IsValid())
	{
		return FeedbackData.Get()->BroadcastDefaultSeconds;
	}

	// SoftRef 로드가 아직 안 됐을 수 있으므로, 로드 시도(클라에서는 필요 없고 서버에서만 의미)
	if (FeedbackData.ToSoftObjectPath().IsValid())
	{
		UMosesFlagFeedbackData* Loaded = FeedbackData.LoadSynchronous();
		if (Loaded)
		{
			return Loaded->BroadcastDefaultSeconds;
		}
	}

	// 폴백: 오늘 정책 기본 4초
	return 4.0f;
}
