#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/AttributeSet/MosesZombieAttributeSet.h"
#include "UE5_Multi_Shooter/Match/Characters/Animation/MosesZombieAnimInstance.h"

#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "AIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectExtension.h"

AMosesZombieCharacter::AMosesZombieCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

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

	NextAttackServerTime = 0.0;
	bIsAttacking_Server = false;

	bIsDying_Server = false;
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

		// [MOD] 서버에서 "SpawnedAttributes에 이 AttributeSet이 들어있는지" 증거 로그
		if (HasAuthority())
		{
			bool bHasZombieAttr = false;
			for (UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes())
			{
				if (Set == AttributeSet)
				{
					bHasZombieAttr = true;
					break;
				}
			}

			UE_LOG(LogMosesZombie, Warning,
				TEXT("[ZOMBIE][GAS][SV] InitActorInfo OK Zombie=%s ASC=%s AttrSet=%s SpawnedHasAttr=%d"),
				*GetName(),
				*GetNameSafe(AbilitySystemComponent),
				*GetNameSafe(AttributeSet),
				bHasZombieAttr ? 1 : 0
			);
		}
	}

	// 클라 비용 절감(서버만 판정)
	if (!HasAuthority())
	{
		if (AttackHitBox_L) { AttackHitBox_L->SetGenerateOverlapEvents(false); }
		if (AttackHitBox_R) { AttackHitBox_R->SetGenerateOverlapEvents(false); }
	}

	if (HasAuthority())
	{
		InitializeAttributes_Server();
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Spawned Zombie=%s"), *GetName());
	}

	// spawn 시 dead는 false로 강제 동기화
	Multicast_SetDeadState(false);
}

void AMosesZombieCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AttachAttackHitBoxesToHandSockets();
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

			if (!MeshComp->DoesSocketExist(SocketName))
			{
				UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][HITBOX] %s Skip - SocketMissing=%s Zombie=%s"),
					SideLabel, *SocketName.ToString(), *GetName());
				return;
			}

			HitBox->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform, SocketName);

			UE_LOG(LogMosesZombie, Log, TEXT("[ZOMBIE][HITBOX] %s Attached Socket=%s Zombie=%s"),
				SideLabel, *SocketName.ToString(), *GetName());
		};

	AttachIfSocketExists(AttackHitBox_L, LeftSocketName, TEXT("L"));
	AttachIfSocketExists(AttackHitBox_R, RightSocketName, TEXT("R"));
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

	// [MOD] 증거: Init 직후 값 확인
	const float CurHP = AbilitySystemComponent->GetNumericAttribute(UMosesZombieAttributeSet::GetHealthAttribute());
	const float CurMax = AbilitySystemComponent->GetNumericAttribute(UMosesZombieAttributeSet::GetMaxHealthAttribute());

	UE_LOG(LogMosesZombie, Warning,
		TEXT("[ZOMBIE][SV] InitAttr HP=%.0f Cur=%.1f Max=%.1f Zombie=%s"),
		MaxHP, CurHP, CurMax, *GetName());
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

float AMosesZombieCharacter::GetAttackRange_Server() const
{
	return ZombieTypeData ? ZombieTypeData->AttackRange : 180.f;
}

float AMosesZombieCharacter::GetMaxChaseDistance_Server() const
{
	return ZombieTypeData ? ZombieTypeData->MaxChaseDistance : 4000.f;
}

bool AMosesZombieCharacter::ServerTryStartAttack_FromAI(AActor* TargetActor)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (bIsDying_Server)
	{
		return false;
	}

	if (!IsValid(TargetActor))
	{
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (Now < NextAttackServerTime)
	{
		return false;
	}

	if (bIsAttacking_Server)
	{
		return false;
	}

	const float CD = ZombieTypeData ? ZombieTypeData->AttackCooldownSeconds : 1.2f;
	NextAttackServerTime = Now + FMath::Max(0.1f, CD);
	bIsAttacking_Server = true;

	ServerStartAttack();
	return true;
}

void AMosesZombieCharacter::ServerStartAttack()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDying_Server)
	{
		bIsAttacking_Server = false;
		SetAttackHitEnabled_Server(EMosesZombieAttackHand::Both, false);
		return;
	}

	UAnimMontage* Montage = PickAttackMontage_Server();
	if (!Montage)
	{
		bIsAttacking_Server = false;
		return;
	}

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		FOnMontageEnded EndDel;
		EndDel.BindUObject(this, &AMosesZombieCharacter::OnAttackMontageEnded_Server);
		AnimInst->Montage_SetEndDelegate(EndDel, Montage);
	}

	Multicast_PlayAttackMontage(Montage);
}

void AMosesZombieCharacter::ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin)
{
	if (!HasAuthority() || bIsDying_Server)
	{
		return;
	}

	if (bEnabled && bResetHitActorsOnBegin)
	{
		ResetHitActorsThisWindow();
	}

	SetAttackHitEnabled_Server(Hand, bEnabled);

	if (!bEnabled)
	{
		bIsAttacking_Server = false;
	}
}

void AMosesZombieCharacter::SetAttackHitEnabled_Server(EMosesZombieAttackHand Hand, bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	const auto SetOne = [bEnabled](UBoxComponent* Box)
		{
			if (!Box) { return; }
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

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
	{
		return ASC;
	}

	if (const APawn* Pawn = Cast<APawn>(TargetActor))
	{
		if (APlayerState* PS = Pawn->GetPlayerState())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
			{
				return ASC;
			}
		}
	}

	return nullptr;
}

bool AMosesZombieCharacter::ApplySetByCallerDamageGE_Server(UAbilitySystemComponent* TargetASC, float DamageAmount) const
{
	if (!HasAuthority() || !TargetASC || DamageAmount <= 0.f)
	{
		return false;
	}

	if (!ZombieTypeData || !ZombieTypeData->DamageGE_SetByCaller)
	{
		return false;
	}

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
	if (!HasAuthority() || bIsDying_Server)
	{
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	const bool bIsLeft = (OverlappedComp == AttackHitBox_L);
	const bool bIsRight = (OverlappedComp == AttackHitBox_R);

	if (bIsLeft && !bAttackHitEnabled_L) { return; }
	if (bIsRight && !bAttackHitEnabled_R) { return; }
	if (!bIsLeft && !bIsRight) { return; }

	const APawn* VictimPawn = Cast<APawn>(OtherActor);
	if (!VictimPawn || !VictimPawn->GetPlayerState())
	{
		return;
	}

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
	if (ApplySetByCallerDamageGE_Server(VictimASC, Damage))
	{
		MarkHitActorThisWindow(OtherActor);
	}
}

void AMosesZombieCharacter::OnAttackMontageEnded_Server(UAnimMontage* Montage, bool /*bInterrupted*/)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsAttacking_Server = false;
	SetAttackHitEnabled_Server(EMosesZombieAttackHand::Both, false);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] AttackMontageEnded Montage=%s Zombie=%s"),
		*GetNameSafe(Montage), *GetName());
}

UMosesZombieAnimInstance* AMosesZombieCharacter::GetZombieAnimInstance() const
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	return Cast<UMosesZombieAnimInstance>(AnimInst);
}

void AMosesZombieCharacter::SetZombieDeadFlag_Local(bool bInDead, const TCHAR* From) const
{
	if (UMosesZombieAnimInstance* ZAnim = GetZombieAnimInstance())
	{
		ZAnim->SetIsDead(bInDead);
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][ANIM] SetIsDead=%d From=%s Zombie=%s"),
			bInDead ? 1 : 0, From, *GetName());
	}
	else
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][ANIM] Missing UMosesZombieAnimInstance From=%s Zombie=%s"),
			From, *GetName());
	}
}

void AMosesZombieCharacter::Multicast_SetDeadState_Implementation(bool bInDead)
{
	SetZombieDeadFlag_Local(bInDead, TEXT("Multicast_SetDeadState"));
}

void AMosesZombieCharacter::HandleDamageAppliedFromGAS_Server(const FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth)
{
	if (!HasAuthority() || bIsDying_Server)
	{
		return;
	}

	// ✅ [MOD] EffectContext에서 KillerPS / Headshot 해석해서 캐싱
	const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();

	LastDamageKillerPS = ResolveKillerPlayerState_FromEffectContext_Server(Ctx);
	bLastDamageHeadshot = ResolveHeadshot_FromEffectContext_Server(Ctx);

	UE_LOG(LogMosesZombie, Warning,
		TEXT("[ZOMBIE][SV] TookDamage Zombie=%s Damage=%.1f NewHP=%.1f KillerPS=%s Headshot=%d"),
		*GetName(),
		AppliedDamage,
		NewHealth,
		*GetNameSafe(LastDamageKillerPS),
		bLastDamageHeadshot ? 1 : 0);

	if (NewHealth <= 0.f)
	{
		HandleDeath_Server();
	}
}

AMosesPlayerState* AMosesZombieCharacter::ResolveKillerPlayerState_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const
{
	// 1) 가장 안전: OriginalInstigator -> Pawn -> PlayerState
	if (const AActor* InstigatorActor = Context.GetOriginalInstigator())
	{
		if (const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
		{
			return InstigatorPawn->GetPlayerState<AMosesPlayerState>();
		}
	}

	// 2) 차선: EffectCauser가 Pawn인 케이스(프로젝트/수류탄 등)
	if (const AActor* CauserActor = Context.GetEffectCauser())
	{
		if (const APawn* CauserPawn = Cast<APawn>(CauserActor))
		{
			return CauserPawn->GetPlayerState<AMosesPlayerState>();
		}
	}

	return nullptr;
}

bool AMosesZombieCharacter::ResolveHeadshot_FromEffectContext_Server(const FGameplayEffectContextHandle& Context) const
{
	// 현재 프로젝트 컨텍스트에 헤드샷 정보 주입이 없으면 false
	// 확장 시: Context.GetHitResult() 기반 또는 Tag 기반으로 처리
	const FHitResult* HR = Context.GetHitResult();
	if (!HR)
	{
		return false;
	}

	return (HR->BoneName == HeadBoneName);
}

void AMosesZombieCharacter::HandleDeath_Server()
{
	if (!HasAuthority() || bIsDying_Server)
	{
		return;
	}

	bIsDying_Server = true;

	// ✅ [MOD] 여기서 “킬 수”를 서버 권위로 확정 (SSOT=PlayerState)
	if (LastDamageKillerPS)
	{
		LastDamageKillerPS->ServerAddZombieKill(1);

		UE_LOG(LogMosesZombie, Warning,
			TEXT("[ZOMBIE][KILL][SV] +1 ZombieKill KillerPS=%s Zombie=%s"),
			*GetNameSafe(LastDamageKillerPS),
			*GetNameSafe(this));
	}
	else
	{
		UE_LOG(LogMosesZombie, Warning,
			TEXT("[ZOMBIE][KILL][SV] No KillerPS (No award) Zombie=%s"),
			*GetNameSafe(this));
	}

	// 공격/충돌 봉인
	bIsAttacking_Server = false;
	SetAttackHitEnabled_Server(EMosesZombieAttackHand::Both, false);

	if (AttackHitBox_L)
	{
		AttackHitBox_L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AttackHitBox_L->SetGenerateOverlapEvents(false);
	}
	if (AttackHitBox_R)
	{
		AttackHitBox_R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AttackHitBox_R->SetGenerateOverlapEvents(false);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	// AnimBP 분기용 플래그
	Multicast_SetDeadState(true);

	// 죽음 몽타주
	UAnimMontage* DeathMontage = ZombieTypeData ? ZombieTypeData->DyingMontage : nullptr;

	if (DeathMontage)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV][DEATH] PlayDeathMontage Zombie=%s Montage=%s"),
			*GetName(), *GetNameSafe(DeathMontage));

		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			FOnMontageEnded EndDel;
			EndDel.BindUObject(this, &AMosesZombieCharacter::OnDeathMontageEnded_Server);
			AnimInst->Montage_SetEndDelegate(EndDel, DeathMontage);
		}

		Multicast_PlayAttackMontage(DeathMontage);
	}
	else
	{
		UE_LOG(LogMosesZombie, Warning,
			TEXT("[ZOMBIE][SV][DEATH] NoDeathMontage -> Destroy in %.2fs Zombie=%s"),
			DestroyDelayAfterDeathMontageSeconds,
			*GetName());

		SetLifeSpan(DestroyDelayAfterDeathMontageSeconds);
	}
}

void AMosesZombieCharacter::OnDeathMontageEnded_Server(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesZombie, Warning,
		TEXT("[ZOMBIE][SV][DEATH] DeathMontageEnded Montage=%s Interrupted=%d -> Destroy in %.2fs Zombie=%s"),
		*GetNameSafe(Montage),
		bInterrupted ? 1 : 0,
		DestroyDelayAfterDeathMontageSeconds,
		*GetName());

	// ✅ 요청: “몽타주 끝난 시점부터 5초 뒤 Destroy”
	SetLifeSpan(FMath::Max(0.05f, DestroyDelayAfterDeathMontageSeconds));
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
