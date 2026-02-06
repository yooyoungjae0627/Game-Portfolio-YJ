// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.cpp
// (FULL - UPDATED)
// - Death: Montage Play -> LifeSpan delayed destroy
// - [NEW] bIsDying_Server guard: block AI attack / overlap / windows
// - Keep only one multicast montage RPC (Multicast_PlayAttackMontage)
// ============================================================================

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
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Animation/AnimInstance.h"
#include "Engine/World.h"

#include "AIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
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

	bIsDying_Server = false; // [NEW]
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

			UE_LOG(LogMosesZombie, Log, TEXT("[ZOMBIE][HITBOX] %s Attached Socket=%s RelLoc=%s RelRot=%s Zombie=%s"),
				SideLabel,
				*SocketName.ToString(),
				*HitBox->GetRelativeLocation().ToString(),
				*HitBox->GetRelativeRotation().ToString(),
				*GetName());
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

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] InitAttr HP=%.0f Zombie=%s"), MaxHP, *GetName());
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

// -------------------------
// AI Helper (Server)
// -------------------------

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
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] TryAttack FAIL (Dying) Zombie=%s"), *GetName());
		return false;
	}

	if (!IsValid(TargetActor))
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] TryAttack FAIL (InvalidTarget) Zombie=%s"), *GetName());
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (Now < NextAttackServerTime)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] TryAttack FAIL (Cooldown) Remain=%.2f Zombie=%s"),
			(float)(NextAttackServerTime - Now), *GetName());
		return false;
	}

	if (bIsAttacking_Server)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] TryAttack FAIL (AlreadyAttacking) Zombie=%s"), *GetName());
		return false;
	}

	const float CD = ZombieTypeData ? ZombieTypeData->AttackCooldownSeconds : 1.2f;
	NextAttackServerTime = Now + FMath::Max(0.1f, CD);
	bIsAttacking_Server = true;

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZAI][SV] AttackStart Target=%s CD=%.2f Zombie=%s"),
		*GetNameSafe(TargetActor), CD, *GetName());

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

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Attack Start Zombie=%s Montage=%s"),
		*GetName(), *GetNameSafe(Montage));

	// [MOD] 서버에서 해당 몽타주에만 EndDelegate 바인딩
	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		// EndDelegate는 "특정 몽타주"에만 걸 수 있다 (Clear 금지)
		FOnMontageEnded EndDel;
		EndDel.BindUObject(this, &AMosesZombieCharacter::OnAttackMontageEnded_Server);
		AnimInst->Montage_SetEndDelegate(EndDel, Montage);

		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Bind MontageEndDelegate OK Zombie=%s AnimInst=%s Montage=%s"),
			*GetName(), *GetNameSafe(AnimInst), *GetNameSafe(Montage));
	}
	else
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Bind MontageEndDelegate FAIL (AnimInst null) Zombie=%s"), *GetName());
	}

	// 멀티캐스트로 재생
	Multicast_PlayAttackMontage(Montage);
}

void AMosesZombieCharacter::ServerSetMeleeAttackWindow(EMosesZombieAttackHand Hand, bool bEnabled, bool bResetHitActorsOnBegin)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDying_Server)
	{
		SetAttackHitEnabled_Server(EMosesZombieAttackHand::Both, false); // [NEW] 혹시라도 열려있으면 닫기
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

	UE_LOG(LogMosesZombie, Verbose, TEXT("[ZOMBIE][SV] AttackWindow Hand=%d Enabled=%s Zombie=%s"),
		static_cast<int32>(Hand),
		bEnabled ? TEXT("true") : TEXT("false"),
		*GetName());
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

	// 1) Actor에 ASC가 직접 달려있으면 그걸 사용
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
	{
		return ASC;
	}

	// 2) Pawn이면 PlayerState의 ASC(너 프로젝트 SSOT)까지 탐색
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
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] Missing DamageGE_SetByCaller Zombie=%s"), *GetName());
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

	// [MOD] 플레이어 Pawn만 타격 (PlayerState 없는 Pawn/좀비는 무시)
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
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] AttackHit SKIP (NoASC) Victim=%s Zombie=%s"),
			*GetNameSafe(OtherActor), *GetName());
		return;
	}

	const float Damage = ZombieTypeData ? ZombieTypeData->MeleeDamage : 10.f;
	const bool bApplied = ApplySetByCallerDamageGE_Server(VictimASC, Damage);

	if (bApplied)
	{
		MarkHitActorThisWindow(OtherActor);

		const TCHAR* HandText = bIsLeft ? TEXT("L") : TEXT("R");
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV] AttackHit Hand=%s Victim=%s Damage=%.0f Zombie=%s"),
			HandText, *GetNameSafe(OtherActor), Damage, *GetName());
	}
}

void AMosesZombieCharacter::OnAttackMontageEnded_Server(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority())
	{
		return;
	}

	// 죽는 중이면 봉인
	if (bIsDying_Server)
	{
		bIsAttacking_Server = false;
		SetAttackHitEnabled_Server(EMosesZombieAttackHand::Both, false);
		return;
	}

	// [MOD] 공격 상태는 몽타주 종료 시 반드시 해제
	bIsAttacking_Server = false;

	// 혹시 윈도우 열려있으면 닫기
	bAttackHitEnabled_L = false;
	bAttackHitEnabled_R = false;
	SetAttackHitEnabled_Server(EMosesZombieAttackHand::Both, false);

	UE_LOG(LogMosesZombie, Warning,
		TEXT("[ZOMBIE][SV] AttackMontageEnded Montage=%s Interrupted=%d -> AttackStateReset Zombie=%s"),
		*GetNameSafe(Montage), bInterrupted ? 1 : 0, *GetName());
}


void AMosesZombieCharacter::HandleDamageAppliedFromGAS_Server(const FGameplayEffectModCallbackData& Data, float AppliedDamage, float NewHealth)
{
	if (!HasAuthority())
	{
		return;
	}

	// [NEW] 이미 죽는 중이면 추가 데미지 콜백은 무시
	if (bIsDying_Server)
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

	const FHitResult* HR = Context.GetHitResult();
	if (!HR)
	{
		return false;
	}

	return (!HeadBoneName.IsNone() && HR->BoneName == HeadBoneName);
}

float AMosesZombieCharacter::ComputeDeathDestroyDelaySeconds_Server(UAnimMontage* DeathMontage) const
{
	const float DataDelay = ZombieTypeData ? ZombieTypeData->DeathDestroyDelaySeconds : 0.f;
	if (DataDelay > 0.f)
	{
		return FMath::Clamp(DataDelay, 0.1f, 10.0f);
	}

	// 0이면 Montage 길이 기반
	const float MontageLen = DeathMontage ? DeathMontage->GetPlayLength() : 0.f;
	if (MontageLen > 0.f)
	{
		return FMath::Clamp(MontageLen, 0.3f, 6.0f);
	}

	return 0.3f;
}

void AMosesZombieCharacter::HandleDeath_Server()
{
	if (!HasAuthority())
	{
		return;
	}

	// [GUARD] 중복 사망 방지
	if (GetLifeSpan() > 0.f || bIsDying_Server)
	{
		UE_LOG(LogMosesZombie, Verbose, TEXT("[ZOMBIE][SV][DEATH] Skip AlreadyDying Zombie=%s LifeSpan=%.2f"),
			*GetName(), GetLifeSpan());
		return;
	}

	bIsDying_Server = true; // [NEW] 이 시점부터 모든 공격/오버랩/데미지 콜백 봉인

	AMosesPlayerState* KillerPS = LastDamageKillerPS;
	const bool bHeadshot = bLastDamageHeadshot;

	UE_LOG(LogMosesKillFeed, Warning, TEXT("[KILLFEED][SV] ZombieKilled By=%s Headshot=%s Zombie=%s"),
		*GetNameSafe(KillerPS),
		bHeadshot ? TEXT("true") : TEXT("false"),
		*GetName());

	// 점수/피드 (서버 확정)
	if (KillerPS)
	{
		KillerPS->ServerAddZombieKill(1);
	}

	PushKillFeed_Server(bHeadshot, KillerPS);

	if (bHeadshot)
	{
		PushHeadshotAnnouncement_Server(KillerPS);
	}

	// 죽는 동안 추가 판정/공격/이동 방지
	bIsAttacking_Server = false;
	bAttackHitEnabled_L = false;
	bAttackHitEnabled_R = false;

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

	// [SV->ALL] 죽음 몽타주 재생 (공용 멀티캐스트 재사용)
	UAnimMontage* DeathMontage = ZombieTypeData ? ZombieTypeData->DyingMontage : nullptr;

	if (DeathMontage)
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV][DEATH] PlayDeathMontage Zombie=%s Montage=%s"),
			*GetName(), *GetNameSafe(DeathMontage));

		Multicast_PlayAttackMontage(DeathMontage);
	}
	else
	{
		UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV][DEATH] NoDeathMontage Zombie=%s (Destroy with delay)"),
			*GetName());
	}

	// Destroy 지연 (애니 보이게)
	const float DestroyDelaySeconds = ComputeDeathDestroyDelaySeconds_Server(DeathMontage);
	SetLifeSpan(DestroyDelaySeconds);

	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][SV][DEATH] ScheduledDestroy Delay=%.2f Zombie=%s"),
		DestroyDelaySeconds, *GetName());
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

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

	const ENetMode NM = GetNetMode();
	UE_LOG(LogMosesZombie, Warning, TEXT("[ZOMBIE][NET] MontagePlay RECV NetMode=%d Zombie=%s Montage=%s Mesh=%s AnimInst=%s"),
		(int32)NM, *GetName(), *GetNameSafe(MontageToPlay), *GetNameSafe(MeshComp), *GetNameSafe(AnimInst));

	if (!AnimInst)
	{
		return;
	}

	AnimInst->Montage_Play(MontageToPlay, 1.f);
}


// ----------------------
// 중복 타격 방지
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
