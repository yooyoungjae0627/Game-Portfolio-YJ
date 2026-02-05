#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AttributeSet/MosesZombieAttributeSet.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h" 

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

AMosesZombieCharacter::AMosesZombieCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	HeadBoneName = TEXT("head");
	LastDamageKillerPS = nullptr;
	bLastDamageHeadshot = false;

	AbilitySystemComponent = CreateDefaultSubobject<UMosesAbilitySystemComponent>(TEXT("ASC_Zombie"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMosesZombieAttributeSet>(TEXT("AS_Zombie"));

	AttackHitBox_L = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackHitBox_L"));
	AttackHitBox_L->SetupAttachment(GetMesh());
	AttackHitBox_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackHitBox_L->SetCollisionObjectType(ECC_WorldDynamic);
	AttackHitBox_L->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackHitBox_L->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackHitBox_L->SetGenerateOverlapEvents(true);
	AttackHitBox_L->OnComponentBeginOverlap.AddDynamic(this, &AMosesZombieCharacter::OnAttackHitOverlapBegin);

	AttackHitBox_R = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackHitBox_R"));
	AttackHitBox_R->SetupAttachment(GetMesh());
	AttackHitBox_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackHitBox_R->SetCollisionObjectType(ECC_WorldDynamic);
	AttackHitBox_R->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackHitBox_R->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackHitBox_R->SetGenerateOverlapEvents(true);
	AttackHitBox_R->OnComponentBeginOverlap.AddDynamic(this, &AMosesZombieCharacter::OnAttackHitOverlapBegin);

	bAttackHitEnabled_L = false;
	bAttackHitEnabled_R = false;
}

void AMosesZombieCharacter::AttachAttackHitBoxesToHandSockets()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][HITBOX] Attach FAIL - Mesh=null Zombie=%s"), *GetName());
		return;
	}

	static const FName LeftSocketName(TEXT("LeftHandSocket"));
	static const FName RightSocketName(TEXT("RightHandSocket"));

	auto AttachIfSocketExists = [this, MeshComp](UBoxComponent* HitBox, const FName SocketName, const TCHAR* SideLabel)
		{
			if (!HitBox)
			{
				UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][HITBOX] %s FAIL - HitBox=null Zombie=%s"), SideLabel, *GetName());
				return;
			}

			// 요구사항: 소켓 없으면 생성 X, 부착 X
			if (!MeshComp->DoesSocketExist(SocketName))
			{
				UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][HITBOX] %s Skip - SocketMissing=%s Zombie=%s"),
					SideLabel, *SocketName.ToString(), *GetName());
				return;
			}

			// ★중요: BP에서 조절한 RelativeLocation/Rotation/Scale 값을 유지해야 하므로 KeepRelativeTransform
			HitBox->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform, SocketName);

			UE_LOG(LogMosesZombie, Log, TEXT("[ZOMBIE][HITBOX] %s Attached (KeepRelative) Socket=%s RelLoc=%s RelRot=%s Zombie=%s"),
				SideLabel,
				*SocketName.ToString(),
				*HitBox->GetRelativeLocation().ToString(),
				*HitBox->GetRelativeRotation().ToString(),
				*GetName());
		};

	AttachIfSocketExists(AttackHitBox_L, LeftSocketName, TEXT("L"));
	AttachIfSocketExists(AttackHitBox_R, RightSocketName, TEXT("R"));
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

	// [MOD] 클라에서는 어차피 판정 안 하므로, 비용 줄이려면 OverlapEvents를 꺼도 됨(선택)
	if (!HasAuthority())
	{
		if (AttackHitBox_L)
		{
			AttackHitBox_L->SetGenerateOverlapEvents(false);
		}
		if (AttackHitBox_R)
		{
			AttackHitBox_R->SetGenerateOverlapEvents(false);
		}
	}

	if (HasAuthority())
	{
		InitializeAttributes_Server();
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawned Zombie=%s"), *GetName());
	}
}

void AMosesZombieCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	AttachAttackHitBoxesToHandSockets();
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

	// 애니 연출은 모든 클라 동일
	Multicast_PlayAttackMontage(Montage);

	// [MOD] 공격 판정 윈도우 ON/OFF는 "AnimNotifyState_MosesZombieAttackWindow"가 제어한다.
	// 즉, 여기서 타이머로 켜고 끄지 않는다.
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

void AMosesZombieCharacter::ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin) // [MOD]
{
	if (!HasAuthority())
	{
		return;
	}

	if (bEnabled && bResetHitActorsOnBegin)
	{
		ResetHitActorsThisWindow();
	}

	SetAttackHitEnabled_Server(Hand, bEnabled);

	UE_LOG(LogMosesZombie, Verbose, TEXT("[ZOMBIE][SV] AttackWindow Hand=%d Enabled=%s Zombie=%s"),
		static_cast<int32>(Hand),
		bEnabled ? TEXT("true") : TEXT("false"),
		*GetName());
}

void AMosesZombieCharacter::SetAttackHitEnabled_Server(EMosesZombieAttackHand Hand, bool bEnabled) // [MOD]
{
	if (!HasAuthority())
	{
		return;
	}

	const auto SetOne = [bEnabled](UBoxComponent* Box)
		{
			if (!Box)
			{
				return;
			}
			Box->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		};

	switch (Hand)
	{
	case EMosesZombieAttackHand::Left:
		bAttackHitEnabled_L = bEnabled;
		SetOne(AttackHitBox_L);
		break;
	case EMosesZombieAttackHand::Right:
		bAttackHitEnabled_R = bEnabled;
		SetOne(AttackHitBox_R);
		break;
	case EMosesZombieAttackHand::Both:
	default:
		bAttackHitEnabled_L = bEnabled;
		bAttackHitEnabled_R = bEnabled;
		SetOne(AttackHitBox_L);
		SetOne(AttackHitBox_R);
		break;
	}
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

	// [MOD] 태그는 SSOT 싱글톤에서 가져오는 게 원칙
	const FGameplayTag DamageTag = FMosesGameplayTags::Get().Data_Damage;

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
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	// 서버만 판정
	if (!HasAuthority())
	{
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// [MOD] 손별 윈도우 체크
	const bool bIsLeft = (OverlappedComp == AttackHitBox_L);
	const bool bIsRight = (OverlappedComp == AttackHitBox_R);

	if (bIsLeft && !bAttackHitEnabled_L)
	{
		return;
	}
	if (bIsRight && !bAttackHitEnabled_R)
	{
		return;
	}
	if (!bIsLeft && !bIsRight)
	{
		// 예외: 다른 컴포넌트에서 들어온 경우 무시
		return;
	}

	// [MOD] 중복 타격 방지(공격 윈도우 동안 1회만)
	if (HasHitActorThisWindow(OtherActor))
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
		MarkHitActorThisWindow(OtherActor);

		const TCHAR* HandText = bIsLeft ? TEXT("L") : TEXT("R");

		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] AttackHit Hand=%s Victim=%s Damage=%.0f Zombie=%s"),
			HandText,
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

	// Headshot -> 공용 Announcement 팝업
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

// ----------------------
// [MOD] 중복 타격 방지
// ----------------------

void AMosesZombieCharacter::ResetHitActorsThisWindow()
{
	HitActorsThisWindow.Reset();
}

bool AMosesZombieCharacter::HasHitActorThisWindow(AActor* Actor) const
{
	return (Actor != nullptr) && HitActorsThisWindow.Contains(Actor);
}

void AMosesZombieCharacter::MarkHitActorThisWindow(AActor* Actor)
{
	if (Actor)
	{
		HitActorsThisWindow.Add(Actor);
	}
}
