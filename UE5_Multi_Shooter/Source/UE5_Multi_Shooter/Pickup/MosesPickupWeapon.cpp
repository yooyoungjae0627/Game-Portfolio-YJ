// ============================================================================
// MosesPickupWeapon.cpp (FULL)  [MOD]
// ============================================================================

#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeaponData.h"

#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesSlotOwnershipComponent.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/UI/Match/MosesPickupPromptWidget.h"

AMosesPickupWeapon::AMosesPickupWeapon()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// ---------------------------------------------------------------------
	// [MOD] Skeletal Mesh Component
	// ---------------------------------------------------------------------
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRenderCustomDepth(false);
	Mesh->bCastDynamicShadow = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(Root);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractSphere->SetGenerateOverlapEvents(true);

	// ---------------------------------------------------------------------
	// [MOD] TraceTarget: LineTrace(ECC_Visibility)로 선택 가능하도록 별도 타겟 제공
	// - Mesh가 NoCollision이어도, 인터렉션 트레이스는 이 박스를 맞춘다.
	// ---------------------------------------------------------------------
	TraceTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("TraceTarget"));
	TraceTarget->SetupAttachment(Root);
	TraceTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TraceTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	TraceTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TraceTarget->SetGenerateOverlapEvents(false);
	TraceTarget->SetBoxExtent(FVector(60.f, 60.f, 80.f));

	// ---------------------------------------------------------------------
	// Prompt Widget Component (World UI)
	// ---------------------------------------------------------------------
	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(Root);

	// 기본 정책: 월드 UI, 액터가 근접 시 Visible로 켠다.
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetTwoSided(true);
	PromptWidgetComponent->SetVisibility(false);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 프롬프트 위치 기본값
	PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
}

void AMosesPickupWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (InteractSphere)
	{
		InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereBeginOverlap);
		InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereEndOverlap);
	}

	// Prompt widget는 기본 Hidden
	SetPromptVisible_Local(false);

	// ---------------------------------------------------------------------
	// [MOD] 월드 스켈레탈 메시 적용(선택)
	// - SoftObjectPtr이므로 LoadSynchronous가 필요할 수 있다.
	// - 여기서는 IsValid()가 true면 Get() 사용(이미 로드된 경우).
	// ---------------------------------------------------------------------
	if (PickupData && PickupData->WorldMesh.IsValid())
	{
		Mesh->SetSkeletalMesh(PickupData->WorldMesh.Get());
	}

	// Prompt 텍스트는 위젯이 준비된 이후 갱신되도록 BeginPlay에서 한 번 시도
	ApplyPromptText_Local();
}

void AMosesPickupWeapon::SetLocalHighlight(bool bEnable)
{
	// 로컬 코스메틱: CustomDepth 토글
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bEnable);
	}
}

bool AMosesPickupWeapon::ServerTryPickup(AMosesPlayerState* RequesterPS, FText& OutAnnounceText)
{
	OutAnnounceText = FText::GetEmpty();

	if (!HasAuthority())
	{
		return false;
	}

	// Guard (기본)
	if (!CanPickup_Server(RequesterPS))
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] FAIL Guard Actor=%s Player=%s"),
			*GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	// 원자성: 이미 소비됨
	if (bConsumed)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP][SV] FAIL AlreadyConsumed Actor=%s"), *GetNameSafe(this));
		return false;
	}

	// 필수 데이터/컴포넌트 검증을 먼저 한다. (실패 시 bConsumed를 켜면 안 된다)
	if (!PickupData)
	{
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL NoPickupData Actor=%s"), *GetNameSafe(this));
		return false;
	}

	UMosesSlotOwnershipComponent* Slots = RequesterPS ? RequesterPS->FindComponentByClass<UMosesSlotOwnershipComponent>() : nullptr;
	if (!Slots)
	{
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL MissingSlots Actor=%s Player=%s"),
			*GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	// ---------------------------------------------------------------------
	// Atomic consume point
	// ---------------------------------------------------------------------
	bConsumed = true;

	// 지급(SSOT) - 기존 로직 유지
	Slots->ServerAcquireSlot(PickupData->SlotIndex, PickupData->ItemId);

	// 성공 Announcement 텍스트 구성
	const FText NameText = !PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->ItemId.ToString());

	OutAnnounceText = FText::Format(
		FText::FromString(TEXT("{0} 획득")),
		NameText);

	UE_LOG(LogMosesPickup, Log, TEXT("[PICKUP][SV] OK Player=%s Slot=%d Item=%s"),
		*GetNameSafe(RequesterPS), PickupData->SlotIndex, *PickupData->ItemId.ToString());

	Destroy(); // 서버 Destroy -> 클라에도 제거
	return true;
}

void AMosesPickupWeapon::HandleSphereBeginOverlap(
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

	// 로컬 UX: 로컬 Pawn만 하이라이트/프롬프트 표시
	if (Pawn->IsLocallyControlled())
	{
		LocalPromptPawn = Pawn;

		SetLocalHighlight(true);

		ApplyPromptText_Local();
		SetPromptVisible_Local(true);

		UE_LOG(LogMosesPickup, Verbose, TEXT("[PICKUP][CL] Prompt Show Actor=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(Pawn));
	}
}

void AMosesPickupWeapon::HandleSphereEndOverlap(
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

	if (Pawn->IsLocallyControlled())
	{
		// 현재 프롬프트 대상이면 숨김
		if (LocalPromptPawn.Get() == Pawn)
		{
			LocalPromptPawn = nullptr;
		}

		SetLocalHighlight(false);
		SetPromptVisible_Local(false);

		UE_LOG(LogMosesPickup, Verbose, TEXT("[PICKUP][CL] Prompt Hide Actor=%s Pawn=%s"),
			*GetNameSafe(this), *GetNameSafe(Pawn));
	}
}

void AMosesPickupWeapon::SetPromptVisible_Local(bool bVisible)
{
	// Prompt는 클라 코스메틱이다. (서버 권위 아님)
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetVisibility(bVisible);
	}
}

void AMosesPickupWeapon::ApplyPromptText_Local()
{
	// Prompt는 클라 코스메틱이지만, 서버에서도 BeginPlay가 도니 안전하게 체크한다.
	if (!PromptWidgetComponent || !PickupData)
	{
		return;
	}

	UUserWidget* Widget = PromptWidgetComponent->GetUserWidgetObject();
	UMosesPickupPromptWidget* Prompt = Cast<UMosesPickupPromptWidget>(Widget);
	if (!Prompt)
	{
		// BP가 UMosesPickupPromptWidget를 Parent로 쓰지 않으면 캐스팅 실패할 수 있다.
		return;
	}

	const FText InteractText = !PickupData->InteractText.IsEmpty()
		? PickupData->InteractText
		: FText::FromString(TEXT("E : 줍기"));

	const FText NameText = !PickupData->DisplayName.IsEmpty()
		? PickupData->DisplayName
		: FText::FromString(PickupData->ItemId.ToString());

	Prompt->SetPromptTexts(InteractText, NameText);
}

bool AMosesPickupWeapon::CanPickup_Server(const AMosesPlayerState* RequesterPS) const
{
	if (!RequesterPS || !PickupData)
	{
		return false;
	}

	// TODO: PhaseReject / DeadReject 등을 여기에 추가 가능
	return true;
}
