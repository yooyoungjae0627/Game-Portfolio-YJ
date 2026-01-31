// ============================================================================
// MosesPickupWeapon.cpp (FULL)  [MOD]
// ============================================================================

#include "UE5_Multi_Shooter/Pickup/MosesPickupWeapon.h"
#include "UE5_Multi_Shooter/Pickup/MosesPickupWeaponData.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"

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

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRenderCustomDepth(false);

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(Root);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractSphere->SetGenerateOverlapEvents(true);

	// ---------------------------------------------------------------------
	// [MOD] Prompt Widget Component (World UI)
	// ---------------------------------------------------------------------
	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(Root);

	// 기본 정책: 월드 UI, 액터가 근접 시 Visible로 켠다.
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetVisibility(false);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMosesPickupWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (InteractSphere)
	{
		InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereBeginOverlap);
		InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AMosesPickupWeapon::HandleSphereEndOverlap);
	}

	// [MOD] Prompt widget는 기본 Hidden
	SetPromptVisible_Local(false);

	// 월드 메쉬 적용(선택)
	if (PickupData && PickupData->WorldMesh.IsValid())
	{
		Mesh->SetStaticMesh(PickupData->WorldMesh.Get());
	}

	// [MOD] Prompt 텍스트는 위젯이 준비된 이후 갱신되도록 BeginPlay에서 한 번 시도
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
		UE_LOG(LogMosesPickup, Error, TEXT("[PICKUP][SV] FAIL MissingSlots Actor=%s Player=%s"), *GetNameSafe(this), *GetNameSafe(RequesterPS));
		return false;
	}

	// ---------------------------------------------------------------------
	// Atomic consume point
	// ---------------------------------------------------------------------
	bConsumed = true;

	// 지급(SSOT) - 기존 로직 유지
	Slots->ServerAcquireSlot(PickupData->SlotIndex, PickupData->ItemId);

	// [MOD] 성공 Announcement 텍스트 구성
	// - 데이터에 DisplayName이 있으면 그걸 사용
	// - 없으면 ItemId 문자열 사용
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

	// ---------------------------------------------------------------------
	// 로컬 UX: 로컬 Pawn만 하이라이트/프롬프트 표시
	// ---------------------------------------------------------------------
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

	// 위젯 인스턴스가 생성되어 있다면 텍스트를 갱신한다.
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
