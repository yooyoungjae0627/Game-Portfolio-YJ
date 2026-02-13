#include "UE5_Multi_Shooter/Match/Pickup/MosesPickupHP.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h" 
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

AMosesPickupHP::AMosesPickupHP()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	RootComponent = Sphere;

	Sphere->InitSphereRadius(80.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMosesPickupHP::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnOverlapBegin);
}

void AMosesPickupHP::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	HandleOverlap_Server(OtherActor);
}

void AMosesPickupHP::HandleOverlap_Server(AActor* OtherActor)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	AMosesPlayerState* PS = Pawn->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	UMosesAbilitySystemComponent* ASC = Cast<UMosesAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!ASC)
	{
		return;
	}

	if (!GE_Heal_SetByCaller)
	{
		UE_LOG(LogMosesPickup, Warning, TEXT("[HP][SV] FAIL (GE_Heal_SetByCaller NULL) Item=%s"), *GetNameSafe(this));
		return;
	}

	const int32 HealAmount = FMath::RandRange(HealMin, HealMax);

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_Heal_SetByCaller, 1.0f, Ctx);
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Heal, (float)HealAmount);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	UE_LOG(LogMosesPickup, Warning, TEXT("[HP][SV] Heal OK Amount=%d PS=%s"), HealAmount, *GetNameSafe(PS));

	// 중앙 방송 4초
	if (AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr)
	{
		const FString Msg = FString::Printf(TEXT("HP +%d"), HealAmount);
		MGS->ServerStartAnnouncementText(FText::FromString(Msg), 4); // 네 GameState API에 맞게 호출명 조정
	}

	Destroy();
}
