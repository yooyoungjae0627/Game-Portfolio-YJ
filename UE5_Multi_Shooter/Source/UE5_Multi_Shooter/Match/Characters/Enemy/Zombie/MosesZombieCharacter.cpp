#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AttributeSet/MosesZombieAttributeSet.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"

#include "Components/BoxComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

AMosesZombieCharacter::AMosesZombieCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	HeadBoneName = TEXT("head");
	bAttackHitEnabled = false;
	bLastDamageHeadshot = false;

	AbilitySystemComponent = CreateDefaultSubobject<UMosesAbilitySystemComponent>(TEXT("ASC_Zombie"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMosesZombieAttributeSet>(TEXT("AS_Zombie"));

	AttackHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackHitBox"));
	AttackHitBox->SetupAttachment(GetMesh());
	AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackHitBox->SetCollisionObjectType(ECC_WorldDynamic);
	AttackHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackHitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackHitBox->SetGenerateOverlapEvents(true);

	AttackHitBox->OnComponentBeginOverlap.AddDynamic(this, &AMosesZombieCharacter::OnAttackHitOverlapBegin);
}

UAbilitySystemComponent* AMosesZombieCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMosesZombieCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	if (HasAuthority())
	{
		InitializeAttributes_Server();
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawned Zombie=%s"), *GetName());
	}
}

void AMosesZombieCharacter::InitializeAttributes_Server()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	const float MaxHP = ZombieTypeData ? ZombieTypeData->MaxHP : 100.f;

	AbilitySystemComponent->SetNumericAttributeBase(UMosesZombieAttributeSet::GetMaxHealthAttribute(), MaxHP);
	AbilitySystemComponent->SetNumericAttributeBase(UMosesZombieAttributeSet::GetHealthAttribute(), MaxHP);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] InitAttr HP=%.0f Zombie=%s"), MaxHP, *GetName());
}

void AMosesZombieCharacter::ServerStartAttack()
{
	if (!HasAuthority())
	{
		return;
	}

	UAnimMontage* Montage = PickAttackMontage_Server();
	if (!Montage)
	{
		return;
	}

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Attack Start Zombie=%s Montage=%s"),
		*GetName(), *GetNameSafe(Montage));

	Multicast_PlayAttackMontage(Montage);

	// 공격 윈도우 ON (Overlap 판정은 서버만)
	SetAttackHitEnabled_Server(true);

	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(
		TimerHandle,
		[this]()
		{
			SetAttackHitEnabled_Server(false);
		},
		0.35f,
		false);
}

UAnimMontage* AMosesZombieCharacter::PickAttackMontage_Server() const
{
	if (!ZombieTypeData || ZombieTypeData->AttackMontages.Num() <= 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, ZombieTypeData->AttackMontages.Num() - 1);
	return ZombieTypeData->AttackMontages[Index];
}

void AMosesZombieCharacter::SetAttackHitEnabled_Server(bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bAttackHitEnabled = bEnabled;
	AttackHitBox->SetCollisionEnabled(bAttackHitEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

UAbilitySystemComponent* AMosesZombieCharacter::FindASCFromActor_Server(AActor* TargetActor) const
{
	if (!HasAuthority() || !TargetActor)
	{
		return nullptr;
	}

	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
}

bool AMosesZombieCharacter::ApplySetByCallerDamageGE_Server(UAbilitySystemComponent* TargetASC, float DamageAmount) const
{
	if (!HasAuthority() || !TargetASC || DamageAmount <= 0.f)
	{
		return false;
	}

	if (!ZombieTypeData || !ZombieTypeData->DamageGE_SetByCaller)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Missing DamageGE_SetByCaller Zombie=%s"), *GetName());
		return false;
	}

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage")));

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(ZombieTypeData->DamageGE_SetByCaller, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageAmount);
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

void AMosesZombieCharacter::OnAttackHitOverlapBegin(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority() || !bAttackHitEnabled)
	{
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	UAbilitySystemComponent* VictimASC = FindASCFromActor_Server(OtherActor);
	if (!VictimASC)
	{
		return;
	}

	const float Damage = ZombieTypeData ? ZombieTypeData->MeleeDamage : 10.f;
	const bool bApplied = ApplySetByCallerDamageGE_Server(VictimASC, Damage);

	if (bApplied)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] AttackHit Player=%s Damage=%.0f Zombie=%s"),
			*GetNameSafe(OtherActor),
			Damage,
			*GetName());
	}
}

void AMosesZombieCharacter::HandleDamageAppliedFromGAS_Server(const FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth)
{
	if (!HasAuthority())
	{
		return;
	}

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();

	LastDamageKillerPS = ResolveKillerPlayerState_FromEffectContext_Server(Context);
	bLastDamageHeadshot = ResolveHeadshot_FromEffectContext_Server(Context);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] TookDamage Zombie=%s Damage=%.0f NewHP=%.0f Killer=%s Headshot=%s"),
		*GetName(),
		AppliedDamage,
		NewHealth,
		*GetNameSafe(LastDamageKillerPS),
		bLastDamageHeadshot ? TEXT("true") : TEXT("false"));

	if (NewHealth <= 0.f)
	{
		HandleDeath_Server();
	}
}

AMosesPlayerState* AMosesZombieCharacter::ResolveKillerPlayerState_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	AActor* InstigatorActor = Context.GetOriginalInstigator();
	if (!InstigatorActor)
	{
		InstigatorActor = Context.GetInstigator();
	}

	APawn* InstigatorPawn = Cast<APawn>(InstigatorActor);
	AController* InstigatorController = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;
	if (!InstigatorController)
	{
		return nullptr;
	}

	return Cast<AMosesPlayerState>(InstigatorController->PlayerState);
}

bool AMosesZombieCharacter::ResolveHeadshot_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const
{
	if (!HasAuthority())
	{
		return false;
	}

	// 무기 서버 트레이스가 EffectContext에 HitResult를 넣어야 동작함(AddHitResult)
	const FHitResult* HR = Context.GetHitResult();
	if (!HR)
	{
		return false;
	}

	return (!HeadBoneName.IsNone() && HR->BoneName == HeadBoneName);
}

void AMosesZombieCharacter::HandleDeath_Server()
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* KillerPS = LastDamageKillerPS;
	const bool bHeadshot = bLastDamageHeadshot;

	UE_LOG(LogMosesKillFeed, Warning, TEXT("[KILLFEED][SV] ZombieKilled By=%s Headshot=%s Zombie=%s"),
		*GetNameSafe(KillerPS),
		bHeadshot ? TEXT("true") : TEXT("false"),
		*GetName());

	if (KillerPS)
	{
		KillerPS->ServerAddZombieKill(1);
	}

	PushKillFeed_Server(bHeadshot, KillerPS);

	// [DAY10][MOD] Headshot -> 공용 Announcement 팝업
	if (bHeadshot)
	{
		PushHeadshotAnnouncement_Server(KillerPS);
	}

	Destroy();
}

void AMosesZombieCharacter::PushKillFeed_Server(bool bHeadshot, AMosesPlayerState* KillerPS)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
	if (!MGS)
	{
		return;
	}

	const FString KillerName = KillerPS ? KillerPS->GetPlayerName() : FString(TEXT("Unknown"));
	const FString Msg = bHeadshot
		? FString::Printf(TEXT("%s Headshot"), *KillerName)
		: FString::Printf(TEXT("%s Zombie Killed"), *KillerName);

	MGS->ServerPushKillFeed(Msg);
}

void AMosesZombieCharacter::PushHeadshotAnnouncement_Server(AMosesPlayerState* KillerPS) const
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
	if (!MGS)
	{
		return;
	}

	FText BaseText = ZombieTypeData ? ZombieTypeData->HeadshotAnnouncementText : FText::FromString(TEXT("헤드샷!"));
	if (BaseText.IsEmpty())
	{
		BaseText = FText::FromString(TEXT("헤드샷!"));
	}

	const FString KillerName = KillerPS ? KillerPS->GetPlayerName() : FString(TEXT("누군가"));
	const FString Msg = FString::Printf(TEXT("%s (%s)"), *BaseText.ToString(), *KillerName);

	const float Sec = ZombieTypeData ? ZombieTypeData->HeadshotAnnouncementSeconds : 0.9f;

	MGS->ServerPushAnnouncement(Msg, FMath::Max(0.2f, Sec));

	UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] HeadshotPopup Msg='%s' Sec=%.2f Killer=%s"),
		*Msg, Sec, *GetNameSafe(KillerPS));
}

void AMosesZombieCharacter::Multicast_PlayAttackMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay)
	{
		return;
	}

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->Montage_Play(MontageToPlay, 1.f);
	}
}
