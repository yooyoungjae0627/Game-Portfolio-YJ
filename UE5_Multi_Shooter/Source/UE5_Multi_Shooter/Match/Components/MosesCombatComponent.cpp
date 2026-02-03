// ============================================================================
// UE5_Multi_Shooter/Combat/MosesCombatComponent.cpp  (FULL - UPDATED)
// ============================================================================
//
// [STEP1 핵심 변경점]
// - ServerGrantWeaponToSlot() 추가: 파밍/획득 시 슬롯 WeaponId 세팅 + 탄약 초기화.
// - Server_EnsureAmmoInitializedForSlot()이 실제로 WeaponData(MagSize/MaxReserve)를 사용해
//   (0/0)인 슬롯 탄약을 초기화하도록 구현.
// - Slot WeaponId 세팅 즉시 RepNotify/Delegate를 갱신하는 내부 함수 추가.
//
// ============================================================================

#include "UE5_Multi_Shooter/Match/Components/MosesCombatComponent.h"

#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesGrenadeProjectile.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"

namespace MosesCombat_Private
{
	static AMosesPlayerState* GetOwnerPS(const UActorComponent* Comp)
	{
		return Comp ? Cast<AMosesPlayerState>(Comp->GetOwner()) : nullptr;
	}

	static APawn* GetOwnerPawn(const UActorComponent* Comp)
	{
		AMosesPlayerState* PS = GetOwnerPS(Comp);
		return PS ? PS->GetPawn() : nullptr;
	}

	static AController* GetOwnerController(const UActorComponent* Comp)
	{
		APawn* Pawn = GetOwnerPawn(Comp);
		return Pawn ? Pawn->GetController() : nullptr;
	}

	static APlayerCharacter* GetOwnerPlayerCharacter(const UActorComponent* Comp)
	{
		return Cast<APlayerCharacter>(GetOwnerPawn(Comp));
	}

	static int32 ClampSlotIndex(int32 SlotIndex)
	{
		return FMath::Clamp(SlotIndex, 1, 4);
	}
}

// ============================================================================
// Ctor / BeginPlay / Replication
// ============================================================================

UMosesCombatComponent::UMosesCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMosesCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[WEAPON][SV] CombatComponent BeginPlay PS=%s"), *GetNameSafe(GetOwner()));
	}
}

void UMosesCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesCombatComponent, CurrentSlot);

	DOREPLIFETIME(UMosesCombatComponent, Slot1WeaponId);
	DOREPLIFETIME(UMosesCombatComponent, Slot2WeaponId);
	DOREPLIFETIME(UMosesCombatComponent, Slot3WeaponId);
	DOREPLIFETIME(UMosesCombatComponent, Slot4WeaponId);

	DOREPLIFETIME(UMosesCombatComponent, Slot1MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot1ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, Slot2MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot2ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, Slot3MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot3ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, Slot4MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot4ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, bIsDead);
	DOREPLIFETIME(UMosesCombatComponent, bIsReloading);

	// SwapContext
	DOREPLIFETIME(UMosesCombatComponent, SwapSerial);
	DOREPLIFETIME(UMosesCombatComponent, LastSwapFromSlot);
	DOREPLIFETIME(UMosesCombatComponent, LastSwapToSlot);
}

// ============================================================================
// SSOT Query
// ============================================================================

FGameplayTag UMosesCombatComponent::GetWeaponIdForSlot(int32 SlotIndex) const
{
	return GetSlotWeaponIdInternal(SlotIndex);
}

FGameplayTag UMosesCombatComponent::GetEquippedWeaponId() const
{
	return GetSlotWeaponIdInternal(CurrentSlot);
}

int32 UMosesCombatComponent::GetCurrentMagAmmo() const
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);
	return Mag;
}

int32 UMosesCombatComponent::GetCurrentReserveAmmo() const
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);
	return Reserve;
}

// ============================================================================
// Default init / loadout (Server)
// ============================================================================

void UMosesCombatComponent::ServerInitDefaultSlots_4(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3, const FGameplayTag& InSlot4)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Slot1WeaponId = InSlot1;
	Slot2WeaponId = InSlot2;
	Slot3WeaponId = InSlot3;
	Slot4WeaponId = InSlot4;

	CurrentSlot = 1;

	// [MOD] 초기 슬롯 무기는 들어오되, 탄약은 "WeaponData 기준 기본값"으로 자동 세팅할 수 있다.
	//       단, Match 기본 지급(라이플 30/90)은 PlayerState에서 별도로 강제한다.
	Server_EnsureAmmoInitializedForSlot(1, Slot1WeaponId);
	Server_EnsureAmmoInitializedForSlot(2, Slot2WeaponId);
	Server_EnsureAmmoInitializedForSlot(3, Slot3WeaponId);
	Server_EnsureAmmoInitializedForSlot(4, Slot4WeaponId);

	bInitialized_DefaultSlots = true;

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] InitDefaultSlots_4 S1=%s S2=%s S3=%s S4=%s"),
		*Slot1WeaponId.ToString(), *Slot2WeaponId.ToString(), *Slot3WeaponId.ToString(), *Slot4WeaponId.ToString());

	BroadcastEquippedChanged(TEXT("ServerInitDefaultSlots_4"));
	BroadcastAmmoChanged(TEXT("ServerInitDefaultSlots_4"));
}

void UMosesCombatComponent::ServerGrantDefaultRifleAmmo_30_90()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!Slot1WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] DefaultRifleAmmo FAIL (Slot1WeaponId invalid)"));
		return;
	}

	SetSlotAmmo_Internal(1, 30, 90);

	CurrentSlot = 1;
	OnRep_CurrentSlot();

	UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] DefaultRifleAmmo Granted Slot=1 Mag=30 Reserve=90 Weapon=%s"),
		*Slot1WeaponId.ToString());
}

// ============================================================================
// [MOD][STEP1] Grant/Pickup API (Server only)
// ============================================================================

void UMosesCombatComponent::ServerGrantWeaponToSlot(int32 SlotIndex, const FGameplayTag& WeaponId, bool bInitializeAmmoIfEmpty /*=true*/)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] GrantWeaponToSlot FAIL (InvalidSlot=%d) PS=%s"),
			SlotIndex, *GetNameSafe(GetOwner()));
		return;
	}

	if (!WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] GrantWeaponToSlot FAIL (InvalidWeaponId) Slot=%d PS=%s"),
			SlotIndex, *GetNameSafe(GetOwner()));
		return;
	}

	const int32 ClampedSlot = MosesCombat_Private::ClampSlotIndex(SlotIndex);

	// 이미 동일 무기면 중복 세팅은 스킵(탄약 초기화는 상황에 따라 필요할 수 있으나, 여기서는 보수적으로 스킵)
	if (GetSlotWeaponIdInternal(ClampedSlot) == WeaponId)
	{
		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][SV] GrantWeaponToSlot SKIP (SameWeapon) Slot=%d Weapon=%s PS=%s"),
			ClampedSlot, *WeaponId.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	// 슬롯 무기 SSOT 갱신
	Server_SetSlotWeaponId_Internal(ClampedSlot, WeaponId);

	// [MOD] 탄약 초기화(0/0이면 WeaponData 기준으로 채움)
	if (bInitializeAmmoIfEmpty)
	{
		Server_EnsureAmmoInitializedForSlot(ClampedSlot, WeaponId);
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] GrantWeaponToSlot OK Slot=%d Weapon=%s (InitAmmo=%d) PS=%s"),
		ClampedSlot, *WeaponId.ToString(), bInitializeAmmoIfEmpty ? 1 : 0, *GetNameSafe(GetOwner()));

	// HUD는 CurrentSlot 기준 표시이므로,
	// 현재 장착 슬롯의 무기가 방금 세팅된 경우 즉시 Ammo 이벤트를 쏴준다.
	if (CurrentSlot == ClampedSlot)
	{
		BroadcastAmmoChanged(TEXT("ServerGrantWeaponToSlot(CurrentSlot)"));
	}

	BroadcastSlotsStateChanged(ClampedSlot, TEXT("ServerGrantWeaponToSlot"));
}

// ============================================================================
// Equip API
// ============================================================================

void UMosesCombatComponent::RequestEquipSlot(int32 SlotIndex)
{
	if (!GetOwner())
	{
		return;
	}

	ServerEquipSlot(SlotIndex);
}

void UMosesCombatComponent::ServerEquipSlot_Implementation(int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bIsDead)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip Reject Dead Slot=%d"), SlotIndex);
		return;
	}

	if (bIsReloading)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip Reject Reloading Slot=%d"), SlotIndex);
		return;
	}

	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip Reject InvalidSlot=%d"), SlotIndex);
		return;
	}

	const int32 NewSlot = MosesCombat_Private::ClampSlotIndex(SlotIndex);

	const FGameplayTag NewWeaponId = GetSlotWeaponIdInternal(NewSlot);
	if (!NewWeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip Reject NoWeaponId Slot=%d"), NewSlot);
		return;
	}

	const int32 OldSlot = CurrentSlot;
	const FGameplayTag OldWeaponId = GetEquippedWeaponId();

	if (OldSlot == NewSlot)
	{
		return;
	}

	// SwapContext 갱신(코스메틱 트리거)
	LastSwapFromSlot = OldSlot;
	LastSwapToSlot = NewSlot;
	SwapSerial++;

	CurrentSlot = NewSlot;

	// [MOD] 스왑 직후, 해당 슬롯의 탄약이 0/0이면 WeaponData 기반으로 초기화되게 보강
	Server_EnsureAmmoInitializedForSlot(CurrentSlot, NewWeaponId);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Swap OK Slot %d -> %d Weapon %s -> %s Serial=%d"),
		OldSlot, CurrentSlot, *OldWeaponId.ToString(), *NewWeaponId.ToString(), SwapSerial);

	// ListenServer 환경/테스트 편의: 서버에서도 즉시 Delegate 발행
	OnRep_SwapSerial();
	OnRep_CurrentSlot();

	BroadcastEquippedChanged(TEXT("ServerEquipSlot"));
	BroadcastAmmoChanged(TEXT("ServerEquipSlot"));
}

// ============================================================================
// Fire API
// ============================================================================

void UMosesCombatComponent::RequestFire()
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		ServerFire_Implementation();
		return;
	}

	ServerFire();
}

void UMosesCombatComponent::ServerFire_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	EMosesFireGuardFailReason Reason = EMosesFireGuardFailReason::None;
	FString Debug;

	if (!Server_CanFire(Reason, Debug))
	{
		UE_LOG(LogMosesCombat, VeryVerbose, TEXT("[FIRE][SV] Reject Guard Reason=%d Debug=%s Slot=%d PS=%s"),
			(int32)Reason, *Debug, CurrentSlot, *GetNameSafe(GetOwner()));
		return;
	}

	FGameplayTag ApprovedWeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(ApprovedWeaponId);
	if (!WeaponData)
	{
		UE_LOG(LogMosesCombat, VeryVerbose, TEXT("[FIRE][SV] Reject(NoWeaponData) Weapon=%s"), *ApprovedWeaponId.ToString());
		return;
	}

	if (!Server_IsFireCooldownReady(WeaponData))
	{
		UE_LOG(LogMosesCombat, VeryVerbose, TEXT("[FIRE][SV] Reject(Cooldown) Slot=%d Weapon=%s"),
			CurrentSlot, *ApprovedWeaponId.ToString());
		return;
	}

	Server_UpdateFireCooldownStamp();

	// ✅ 핵심: "현재 슬롯"의 탄약만 소비한다.
	Server_ConsumeAmmo_OnApprovedFire(WeaponData);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Fire Weapon=%s Slot=%d Mag=%d Reserve=%d"),
		*ApprovedWeaponId.ToString(), CurrentSlot, GetCurrentMagAmmo(), GetCurrentReserveAmmo());

	Server_PerformFireAndApplyDamage(WeaponData);

	// 코스메틱은 서버 승인 후 전파
	Server_PropagateFireCosmetics(ApprovedWeaponId);

	BroadcastSlotsStateChanged(CurrentSlot, TEXT("ServerFire"));
}

// ============================================================================
// Reload API
// ============================================================================

void UMosesCombatComponent::RequestReload()
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		ServerReload_Implementation();
		return;
	}

	ServerReload();
}

void UMosesCombatComponent::ServerReload_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bIsDead || bIsReloading)
	{
		return;
	}

	FGameplayTag WeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(WeaponId);
	if (!WeaponData)
	{
		return;
	}

	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	if (Reserve <= 0 || Mag >= WeaponData->MagSize)
	{
		return;
	}

	Server_StartReload(WeaponData);
}

void UMosesCombatComponent::Server_StartReload(const UMosesWeaponData* WeaponData)
{
	if (!WeaponData)
	{
		return;
	}

	bIsReloading = true;
	OnRep_IsReloading();

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] ReloadStart Slot=%d Weapon=%s Sec=%.2f"),
		CurrentSlot, *GetEquippedWeaponId().ToString(), WeaponData->ReloadSeconds);

	GetWorld()->GetTimerManager().SetTimer(
		ReloadTimerHandle,
		this,
		&UMosesCombatComponent::Server_FinishReload,
		WeaponData->ReloadSeconds,
		false);
}

void UMosesCombatComponent::Server_FinishReload()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FGameplayTag WeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(WeaponId);
	if (!WeaponData)
	{
		bIsReloading = false;
		OnRep_IsReloading();
		return;
	}

	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	const int32 Need = FMath::Max(WeaponData->MagSize - Mag, 0);
	const int32 Give = FMath::Min(Need, Reserve);

	Mag += Give;
	Reserve -= Give;

	SetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	bIsReloading = false;
	OnRep_IsReloading();

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] ReloadFinish Slot=%d Weapon=%s Mag=%d Reserve=%d"),
		CurrentSlot, *WeaponId.ToString(), Mag, Reserve);
}

// ============================================================================
// Guard (basic)
// ============================================================================

bool UMosesCombatComponent::Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const
{
	if (bIsDead)
	{
		OutReason = EMosesFireGuardFailReason::IsDead;
		OutDebug = TEXT("Dead");
		return false;
	}

	if (bIsReloading)
	{
		OutReason = EMosesFireGuardFailReason::InvalidPhase;
		OutDebug = TEXT("Reloading");
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutReason = EMosesFireGuardFailReason::NoOwner;
		OutDebug = TEXT("NoOwner");
		return false;
	}

	const APawn* OwnerPawn = MosesCombat_Private::GetOwnerPawn(this);
	if (!OwnerPawn)
	{
		OutReason = EMosesFireGuardFailReason::NoPawn;
		OutDebug = TEXT("NoPawn(PS->GetPawn null)");
		return false;
	}

	if (!OwnerPawn->GetController())
	{
		OutReason = EMosesFireGuardFailReason::NoController;
		OutDebug = TEXT("NoController");
		return false;
	}

	const FGameplayTag WeaponId = GetEquippedWeaponId();
	if (!WeaponId.IsValid())
	{
		OutReason = EMosesFireGuardFailReason::NoWeaponId;
		OutDebug = TEXT("NoWeaponId");
		return false;
	}

	// ✅ 핵심: 현재 슬롯 탄창(Mag)만 체크한다.
	if (GetCurrentMagAmmo() <= 0)
	{
		OutReason = EMosesFireGuardFailReason::NoAmmo;
		OutDebug = TEXT("NoAmmo");
		return false;
	}

	return true;
}

// ============================================================================
// WeaponData Resolve
// ============================================================================

const UMosesWeaponData* UMosesCombatComponent::Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const
{
	OutWeaponId = GetEquippedWeaponId();
	if (!OutWeaponId.IsValid())
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return nullptr;
	}

	UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return nullptr;
	}

	return Registry->ResolveWeaponData(OutWeaponId);
}

// ============================================================================
// Ammo Consume
// ============================================================================

void UMosesCombatComponent::Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* /*WeaponData*/)
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	Mag = FMath::Max(Mag - 1, 0);
	SetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	BroadcastAmmoChanged(TEXT("Server_ConsumeAmmo_OnApprovedFire"));
}

// ============================================================================
// Cooldown (DefaultFireIntervalSec based)
// ============================================================================

float UMosesCombatComponent::Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* /*WeaponData*/) const
{
	return DefaultFireIntervalSec;
}

bool UMosesCombatComponent::Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	const int32 Index = MosesCombat_Private::ClampSlotIndex(CurrentSlot) - 1;
	const double Last = SlotLastFireTimeSec[Index];

	const float IntervalSec = Server_GetFireIntervalSec_FromWeaponData(WeaponData);
	return (Now - Last) >= (double)IntervalSec;
}

void UMosesCombatComponent::Server_UpdateFireCooldownStamp()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	const int32 Index = MosesCombat_Private::ClampSlotIndex(CurrentSlot) - 1;
	SlotLastFireTimeSec[Index] = Now;
}

// ============================================================================
// Fire 수행(스프레드 적용 + 히트스캔/프로젝트 분기)
// ============================================================================

float UMosesCombatComponent::Server_CalcSpreadFactor01(const UMosesWeaponData* WeaponData, const APawn* OwnerPawn) const
{
	if (!WeaponData || !OwnerPawn)
	{
		return 0.0f;
	}

	const float Speed2D = OwnerPawn->GetVelocity().Size2D();
	const float Ref = FMath::Max(1.0f, WeaponData->SpreadSpeedRef);
	return FMath::Clamp(Speed2D / Ref, 0.0f, 1.0f);
}

FVector UMosesCombatComponent::Server_ApplySpreadToDirection(const FVector& AimDir, const UMosesWeaponData* WeaponData, float SpreadFactor01, float& OutHalfAngleDeg) const
{
	if (!WeaponData)
	{
		OutHalfAngleDeg = 0.0f;
		return AimDir.GetSafeNormal();
	}

	OutHalfAngleDeg = FMath::Lerp(WeaponData->SpreadDegrees_Min, WeaponData->SpreadDegrees_Max, SpreadFactor01);
	const float HalfAngleRad = FMath::DegreesToRadians(FMath::Max(0.0f, OutHalfAngleDeg));

	return FMath::VRandCone(AimDir.GetSafeNormal(), HalfAngleRad);
}

void UMosesCombatComponent::Server_PerformFireAndApplyDamage(const UMosesWeaponData* WeaponData)
{
	APawn* OwnerPawn = MosesCombat_Private::GetOwnerPawn(this);
	if (!OwnerPawn)
	{
		return;
	}

	AController* Controller = OwnerPawn->GetController();
	if (!Controller)
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector AimDir = ViewRot.Vector();

	const float SpreadFactor = Server_CalcSpreadFactor01(WeaponData, OwnerPawn);

	float HalfAngleDeg = 0.0f;
	const FVector SpreadDir = Server_ApplySpreadToDirection(AimDir, WeaponData, SpreadFactor, HalfAngleDeg);

	UE_LOG(LogMosesCombat, Warning, TEXT("[AIM][SV] SpreadApplied Factor=%.2f HalfAngleDeg=%.2f Speed=%.1f Weapon=%s"),
		SpreadFactor,
		HalfAngleDeg,
		OwnerPawn->GetVelocity().Size2D(),
		WeaponData ? *WeaponData->WeaponId.ToString() : TEXT("None"));

	// Projectile weapon (Grenade Launcher)
	if (WeaponData->bIsProjectileWeapon)
	{
		Server_SpawnGrenadeProjectile(WeaponData, ViewLoc, SpreadDir, Controller, OwnerPawn);
		return;
	}

	const FVector Start = ViewLoc;
	const FVector End = Start + (SpreadDir * HitscanDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Moses_FireTrace), true);
	Params.AddIgnoredActor(OwnerPawn);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FireTraceChannel, Params);
	if (!bHit || !Hit.GetActor())
	{
		UE_LOG(LogMosesCombat, Verbose, TEXT("[HIT][SV] Miss"));
		return;
	}

	const bool bHeadshot = (Hit.BoneName == HeadshotBoneName);
	const float BaseDamage = WeaponData ? WeaponData->Damage : DefaultDamage;
	const float AppliedDamage = BaseDamage * (bHeadshot ? HeadshotDamageMultiplier : 1.0f);

	UE_LOG(LogMosesCombat, Warning, TEXT("[HIT][SV] Victim=%s Bone=%s Headshot=%d Damage=%.1f"),
		*GetNameSafe(Hit.GetActor()), *Hit.BoneName.ToString(), bHeadshot ? 1 : 0, AppliedDamage);

	const bool bAppliedByGAS = Server_ApplyDamageToTarget_GAS(Hit.GetActor(), AppliedDamage, Controller, OwnerPawn, WeaponData);
	if (!bAppliedByGAS)
	{
		UGameplayStatics::ApplyDamage(Hit.GetActor(), AppliedDamage, Controller, OwnerPawn, nullptr);

		UE_LOG(LogMosesCombat, Warning, TEXT("[HIT][SV] Fallback ApplyDamage Victim=%s Damage=%.1f"),
			*GetNameSafe(Hit.GetActor()), AppliedDamage);
	}
}

void UMosesCombatComponent::Server_SpawnGrenadeProjectile(const UMosesWeaponData* WeaponData, const FVector& SpawnLoc, const FVector& FireDir, AController* InstigatorController, APawn* OwnerPawn)
{
	if (!WeaponData || !OwnerPawn)
	{
		return;
	}

	if (!GrenadeProjectileClass)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GRENADE][SV] Spawn FAIL (GrenadeProjectileClass is null)"));
		return;
	}

	AMosesPlayerState* PS = MosesCombat_Private::GetOwnerPS(this);
	if (!PS)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GRENADE][SV] Spawn FAIL (No PlayerState owner)"));
		return;
	}

	UAbilitySystemComponent* SourceASC = PS->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GRENADE][SV] Spawn FAIL (No Source ASC)"));
		return;
	}

	TSubclassOf<UGameplayEffect> GEClass = DamageGE_SetByCaller.LoadSynchronous();
	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] DamageGE_SetByCaller is null (load failed)"));
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = PS;
	Params.Instigator = OwnerPawn;

	const FRotator Rot = FireDir.Rotation();

	AMosesGrenadeProjectile* Projectile = GetWorld()->SpawnActor<AMosesGrenadeProjectile>(GrenadeProjectileClass, SpawnLoc, Rot, Params);
	if (!Projectile)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GRENADE][SV] Spawn FAIL (SpawnActor returned null)"));
		return;
	}

	const float ExplodeDamage = (WeaponData->ExplosionDamageOverride > 0.0f) ? WeaponData->ExplosionDamageOverride : WeaponData->Damage;

	Projectile->InitFromCombat_Server(
		SourceASC,
		GEClass,
		ExplodeDamage,
		WeaponData->ExplosionRadius,
		InstigatorController,
		OwnerPawn,
		WeaponData);

	Projectile->Launch_Server(FireDir, WeaponData->ProjectileSpeed);

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[GRENADE][SV] Spawn OK Weapon=%s Speed=%.1f Radius=%.1f Damage=%.1f"),
		*WeaponData->WeaponId.ToString(),
		WeaponData->ProjectileSpeed,
		WeaponData->ExplosionRadius,
		ExplodeDamage);
}

// ============================================================================
// GAS Damage (히트스캔 + 유탄 공통 파이프라인)
// ============================================================================

bool UMosesCombatComponent::Server_ApplyDamageToTarget_GAS(AActor* TargetActor, float Damage, AController* InstigatorController, AActor* DamageCauser, const UMosesWeaponData* WeaponData) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NotAuthority)"));
		return false;
	}

	if (!TargetActor)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (Target=null)"));
		return false;
	}

	AMosesPlayerState* PS = MosesCombat_Private::GetOwnerPS(this);
	if (!PS)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NoOwnerPS)"));
		return false;
	}

	UAbilitySystemComponent* SourceASC = PS->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NoSourceASC) PS=%s"), *GetNameSafe(PS));
		return false;
	}

	IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(TargetActor);
	if (!TargetASI)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (TargetNoASI) Target=%s"), *GetNameSafe(TargetActor));
		return false;
	}

	UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (TargetNoASC) Target=%s"), *GetNameSafe(TargetActor));
		return false;
	}

	TSubclassOf<UGameplayEffect> GEClass = DamageGE_SetByCaller.LoadSynchronous();
	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Error, TEXT("[GAS][SV] APPLY FAIL (NoDamageGE) SoftPath=%s"),
			*DamageGE_SetByCaller.ToSoftObjectPath().ToString());
		return false;
	}

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddInstigator(DamageCauser, InstigatorController);
	Ctx.AddSourceObject(this);
	Ctx.AddSourceObject(WeaponData);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEClass, 1.f, Ctx);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (SpecInvalid) Target=%s"), *GetNameSafe(TargetActor));
		return false;
	}

	// NOTE:
	// - 현재 프로젝트에서는 "Health를 Add로 깎는" GE를 쓰는 경로를 가정하고 SignedDamage(음수)를 주입한다.
	// - GE 설계가 바뀌면 여기의 부호 정책도 함께 변경되어야 한다.
	const float SignedDamage = -FMath::Abs(Damage);

	SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Damage, SignedDamage);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY OK Target=%s Damage=%.1f Signed=%.1f Weapon=%s"),
		*GetNameSafe(TargetActor),
		Damage,
		SignedDamage,
		WeaponData ? *WeaponData->WeaponId.ToString() : TEXT("None"));

	return true;
}

// ============================================================================
// Cosmetic propagation (Fire only - already server approved by ServerFire)
// ============================================================================

void UMosesCombatComponent::Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId)
{
	APlayerCharacter* PlayerChar = MosesCombat_Private::GetOwnerPlayerCharacter(this);
	if (!PlayerChar)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][SV] PropagateCosmetic FAIL (No Pawn) PS=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	PlayerChar->Multicast_PlayFireMontage(ApprovedWeaponId);
}

// ============================================================================
// Dead Hook
// ============================================================================

void UMosesCombatComponent::ServerMarkDead()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnRep_IsDead();
	BroadcastDeadChanged(TEXT("ServerMarkDead"));
}

// ============================================================================
// RepNotifies
// ============================================================================

void UMosesCombatComponent::OnRep_CurrentSlot()
{
	BroadcastEquippedChanged(TEXT("OnRep_CurrentSlot"));
	BroadcastAmmoChanged(TEXT("OnRep_CurrentSlot"));
	BroadcastSlotsStateChanged(0, TEXT("OnRep_CurrentSlot"));
}

void UMosesCombatComponent::OnRep_Slot1WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot1WeaponId"));
	BroadcastSlotsStateChanged(1, TEXT("OnRep_Slot1WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot2WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot2WeaponId"));
	BroadcastSlotsStateChanged(2, TEXT("OnRep_Slot2WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot3WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot3WeaponId"));
	BroadcastSlotsStateChanged(3, TEXT("OnRep_Slot3WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot4WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot4WeaponId"));
	BroadcastSlotsStateChanged(4, TEXT("OnRep_Slot4WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot1Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot1Ammo"));
	BroadcastSlotsStateChanged(1, TEXT("OnRep_Slot1Ammo"));
}

void UMosesCombatComponent::OnRep_Slot2Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot2Ammo"));
	BroadcastSlotsStateChanged(2, TEXT("OnRep_Slot2Ammo"));
}

void UMosesCombatComponent::OnRep_Slot3Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot3Ammo"));
	BroadcastSlotsStateChanged(3, TEXT("OnRep_Slot3Ammo"));
}

void UMosesCombatComponent::OnRep_Slot4Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot4Ammo"));
	BroadcastSlotsStateChanged(4, TEXT("OnRep_Slot4Ammo"));
}

void UMosesCombatComponent::OnRep_IsReloading()
{
	BroadcastReloadingChanged(TEXT("OnRep_IsReloading"));
	BroadcastSlotsStateChanged(CurrentSlot, TEXT("OnRep_IsReloading"));
}

void UMosesCombatComponent::OnRep_IsDead() { BroadcastDeadChanged(TEXT("OnRep_IsDead")); }

void UMosesCombatComponent::OnRep_SwapSerial()
{
	BroadcastSwapStarted(TEXT("OnRep_SwapSerial"));
}

// ============================================================================
// Broadcast helpers
// ============================================================================

void UMosesCombatComponent::BroadcastEquippedChanged(const TCHAR* ContextTag)
{
	const FGameplayTag WeaponId = GetEquippedWeaponId();
	OnEquippedChanged.Broadcast(CurrentSlot, WeaponId);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][REP] %s Slot=%d Weapon=%s"),
		ContextTag ? ContextTag : TEXT("None"), CurrentSlot, *WeaponId.ToString());
}

void UMosesCombatComponent::BroadcastAmmoChanged(const TCHAR* ContextTag)
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	OnAmmoChanged.Broadcast(Mag, Reserve);

	UE_LOG(LogMosesCombat, Warning, TEXT("[AMMO][REP] %s Slot=%d Mag=%d Reserve=%d"),
		ContextTag ? ContextTag : TEXT("None"), CurrentSlot, Mag, Reserve);
}

void UMosesCombatComponent::BroadcastDeadChanged(const TCHAR* ContextTag)
{
	OnDeadChanged.Broadcast(bIsDead);

	UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][REP] %s bIsDead=%d"),
		ContextTag ? ContextTag : TEXT("None"), bIsDead ? 1 : 0);
}

void UMosesCombatComponent::BroadcastReloadingChanged(const TCHAR* ContextTag)
{
	OnReloadingChanged.Broadcast(bIsReloading);

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[RELOAD][REP] %s Reloading=%d Slot=%d"),
		ContextTag ? ContextTag : TEXT("None"), bIsReloading ? 1 : 0, CurrentSlot);
}

void UMosesCombatComponent::BroadcastSwapStarted(const TCHAR* ContextTag)
{
	const int32 FromSlot = MosesCombat_Private::ClampSlotIndex(LastSwapFromSlot);
	const int32 ToSlot = MosesCombat_Private::ClampSlotIndex(LastSwapToSlot);

	OnSwapStarted.Broadcast(FromSlot, ToSlot, SwapSerial);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[SWAP][REP] %s From=%d To=%d Serial=%d PS=%s"),
		ContextTag ? ContextTag : TEXT("None"),
		FromSlot,
		ToSlot,
		SwapSerial,
		*GetNameSafe(GetOwner()));
}

// ============================================================================
// Slot helpers
// ============================================================================

bool UMosesCombatComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 1 && SlotIndex <= 4;
}

FGameplayTag UMosesCombatComponent::GetSlotWeaponIdInternal(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 1: return Slot1WeaponId;
	case 2: return Slot2WeaponId;
	case 3: return Slot3WeaponId;
	case 4: return Slot4WeaponId;
	default: break;
	}
	return FGameplayTag();
}

void UMosesCombatComponent::Server_SetSlotWeaponId_Internal(int32 SlotIndex, const FGameplayTag& WeaponId)
{
	check(GetOwner() && GetOwner()->HasAuthority());

	switch (SlotIndex)
	{
	case 1:
		Slot1WeaponId = WeaponId;
		OnRep_Slot1WeaponId();
		break;
	case 2:
		Slot2WeaponId = WeaponId;
		OnRep_Slot2WeaponId();
		break;
	case 3:
		Slot3WeaponId = WeaponId;
		OnRep_Slot3WeaponId();
		break;
	case 4:
		Slot4WeaponId = WeaponId;
		OnRep_Slot4WeaponId();
		break;
	default:
		break;
	}
}

// ============================================================================
// Ammo helpers
// ============================================================================

void UMosesCombatComponent::Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!IsValidSlotIndex(SlotIndex) || !WeaponId.IsValid())
	{
		return;
	}

	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Reserve);

	// 이미 탄약이 들어있으면 유지한다(파밍/스왑/리로드 등으로 갱신된 상태 보호)
	if (Mag > 0 || Reserve > 0)
	{
		return;
	}

	// [MOD][STEP1] WeaponData 기반 초기 탄약 세팅
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UMosesWeaponRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UMosesWeaponRegistrySubsystem>() : nullptr;

	const UMosesWeaponData* Data = Registry ? Registry->ResolveWeaponData(WeaponId) : nullptr;
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] EnsureAmmo FAIL (NoWeaponData) Slot=%d Weapon=%s PS=%s"),
			SlotIndex, *WeaponId.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	// 기본 정책: Mag=MagSize, Reserve=MaxReserve
	const int32 InitMag = FMath::Max(0, Data->MagSize);
	const int32 InitReserve = FMath::Max(0, Data->MaxReserve);

	SetSlotAmmo_Internal(SlotIndex, InitMag, InitReserve);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] EnsureAmmo INIT Slot=%d Weapon=%s Ammo=%d/%d PS=%s"),
		SlotIndex, *WeaponId.ToString(), InitMag, InitReserve, *GetNameSafe(GetOwner()));

	// CurrentSlot이면 HUD 전환 즉시 반영
	if (CurrentSlot == SlotIndex)
	{
		BroadcastAmmoChanged(TEXT("Server_EnsureAmmoInitializedForSlot(CurrentSlot)"));
	}
}

void UMosesCombatComponent::GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserve) const
{
	OutMag = 0;
	OutReserve = 0;

	switch (SlotIndex)
	{
	case 1: OutMag = Slot1MagAmmo; OutReserve = Slot1ReserveAmmo; return;
	case 2: OutMag = Slot2MagAmmo; OutReserve = Slot2ReserveAmmo; return;
	case 3: OutMag = Slot3MagAmmo; OutReserve = Slot3ReserveAmmo; return;
	case 4: OutMag = Slot4MagAmmo; OutReserve = Slot4ReserveAmmo; return;
	default: break;
	}
}

void UMosesCombatComponent::SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserve)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	NewMag = FMath::Max(NewMag, 0);
	NewReserve = FMath::Max(NewReserve, 0);

	switch (SlotIndex)
	{
	case 1:
		Slot1MagAmmo = NewMag;
		Slot1ReserveAmmo = NewReserve;
		OnRep_Slot1Ammo();
		return;

	case 2:
		Slot2MagAmmo = NewMag;
		Slot2ReserveAmmo = NewReserve;
		OnRep_Slot2Ammo();
		return;

	case 3:
		Slot3MagAmmo = NewMag;
		Slot3ReserveAmmo = NewReserve;
		OnRep_Slot3Ammo();
		return;

	case 4:
		Slot4MagAmmo = NewMag;
		Slot4ReserveAmmo = NewReserve;
		OnRep_Slot4Ammo();
		return;

	default:
		break;
	}
}

int32 UMosesCombatComponent::GetMagAmmoForSlot(int32 SlotIndex) const
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Reserve);
	return Mag;
}

int32 UMosesCombatComponent::GetReserveAmmoForSlot(int32 SlotIndex) const
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Reserve);
	return Reserve;
}

void UMosesCombatComponent::BroadcastSlotsStateChanged(int32 ChangedSlotOr0ForAll, const TCHAR* ContextTag)
{
	// 0 = 전체 갱신, 1~4 = 해당 슬롯 중심 갱신
	OnSlotsStateChanged.Broadcast(ChangedSlotOr0ForAll);

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][HUD] SlotsStateChanged Slot=%d Ctx=%s PS=%s"),
		ChangedSlotOr0ForAll,
		ContextTag ? ContextTag : TEXT("None"),
		*GetNameSafe(GetOwner()));
}
