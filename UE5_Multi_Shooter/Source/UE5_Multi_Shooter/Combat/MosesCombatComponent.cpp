#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponRegistrySubsystem.h"

UMosesCombatComponent::UMosesCombatComponent()
{
	SetIsReplicatedByDefault(true);
}

void UMosesCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesCombatComponent, CurrentSlot);

	DOREPLIFETIME(UMosesCombatComponent, Slot1WeaponId);
	DOREPLIFETIME(UMosesCombatComponent, Slot2WeaponId);
	DOREPLIFETIME(UMosesCombatComponent, Slot3WeaponId);

	DOREPLIFETIME(UMosesCombatComponent, Slot1MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot1ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, Slot2MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot2ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, Slot3MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot3ReserveAmmo);

	DOREPLIFETIME(UMosesCombatComponent, bIsDead);
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
// Equip API
// ============================================================================
void UMosesCombatComponent::RequestEquipSlot(int32 SlotIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 클라는 요청만, 서버가 결정
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

	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip Reject InvalidSlot=%d"), SlotIndex);
		return;
	}

	const FGameplayTag NewWeaponId = GetSlotWeaponIdInternal(SlotIndex);
	if (!NewWeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip Reject NoWeaponId Slot=%d"), SlotIndex);
		return;
	}

	CurrentSlot = SlotIndex;

	// 슬롯 탄약이 아직 비어 있으면 서버에서 최소값 보장
	Server_EnsureAmmoInitializedForSlot(CurrentSlot, NewWeaponId);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] Equip OK Slot=%d Weapon=%s"),
		CurrentSlot, *NewWeaponId.ToString());

	BroadcastEquippedChanged(TEXT("ServerEquipSlot"));
	BroadcastAmmoChanged(TEXT("ServerEquipSlot"));
}

void UMosesCombatComponent::ServerInitDefaultSlots(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Slot1WeaponId = InSlot1;
	Slot2WeaponId = InSlot2;
	Slot3WeaponId = InSlot3;

	CurrentSlot = 1;

	Server_EnsureAmmoInitializedForSlot(1, Slot1WeaponId);
	Server_EnsureAmmoInitializedForSlot(2, Slot2WeaponId);
	Server_EnsureAmmoInitializedForSlot(3, Slot3WeaponId);

	bInitialized_Day2 = true;

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] InitDefaultSlots S1=%s S2=%s S3=%s"),
		*Slot1WeaponId.ToString(), *Slot2WeaponId.ToString(), *Slot3WeaponId.ToString());

	BroadcastEquippedChanged(TEXT("ServerInitDefaultSlots"));
	BroadcastAmmoChanged(TEXT("ServerInitDefaultSlots"));
}

void UMosesCombatComponent::Server_EnsureInitialized_Day2()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bInitialized_Day2)
	{
		// 이미 초기화 끝
		return;
	}

	const bool bHasAnyWeapon =
		Slot1WeaponId.IsValid() || Slot2WeaponId.IsValid() || Slot3WeaponId.IsValid();

	if (!bHasAnyWeapon)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] EnsureInit_Day2: No default weapon ids set yet (skip)"));
		return;
	}

	if (!IsValidSlotIndex(CurrentSlot))
	{
		CurrentSlot = 1;
	}

	Server_EnsureAmmoInitializedForSlot(1, Slot1WeaponId);
	Server_EnsureAmmoInitializedForSlot(2, Slot2WeaponId);
	Server_EnsureAmmoInitializedForSlot(3, Slot3WeaponId);

	bInitialized_Day2 = true;

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] EnsureInit_Day2 OK Slot=%d"), CurrentSlot);

	BroadcastEquippedChanged(TEXT("Server_EnsureInitialized_Day2"));
	BroadcastAmmoChanged(TEXT("Server_EnsureInitialized_Day2"));
}

// ============================================================================
// [DAY3] Fire API
// ============================================================================
void UMosesCombatComponent::RequestFire()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 클라는 요청만, 서버가 승인/거절
	ServerFire();
}

void UMosesCombatComponent::ServerFire_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesCombat, Log, TEXT("[FIRE][SV] Req Owner=%s Slot=%d"), *GetNameSafe(GetOwner()), CurrentSlot);

	EMosesFireGuardFailReason FailReason = EMosesFireGuardFailReason::None;
	FString Debug;
	if (!Server_CanFire(FailReason, Debug))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=%d Debug=%s"), (int32)FailReason, *Debug);
		return;
	}

	FGameplayTag EquippedWeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(EquippedWeaponId);
	if (!WeaponData)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=%d Debug=WeaponDataMissing Weapon=%s"),
			(int32)EMosesFireGuardFailReason::NoWeaponData, *EquippedWeaponId.ToString());
		return;
	}

	// 서버 승인 → 쿨다운 stamp 갱신 → 탄약 차감 → 히트스캔/데미지
	Server_UpdateFireCooldownStamp();
	Server_ConsumeAmmo_OnApprovedFire(WeaponData);
	Server_PerformHitscanAndApplyDamage(WeaponData);

	UE_LOG(LogMosesCombat, Log, TEXT("[FIRE][SV] Approved Weapon=%s Mag=%d Reserve=%d"),
		*EquippedWeaponId.ToString(), GetCurrentMagAmmo(), GetCurrentReserveAmmo());
}

// ============================================================================
// [DAY3] Guard
// ============================================================================
bool UMosesCombatComponent::Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const
{
	if (bIsDead)
	{
		OutReason = EMosesFireGuardFailReason::IsDead;
		OutDebug = TEXT("Dead");
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutReason = EMosesFireGuardFailReason::NoOwner;
		OutDebug = TEXT("NoOwner");
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!OwnerPawn)
	{
		OutReason = EMosesFireGuardFailReason::NoPawn;
		OutDebug = TEXT("OwnerNotPawn");
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

	if (!Server_IsFireCooldownReady(nullptr))
	{
		OutReason = EMosesFireGuardFailReason::Cooldown;
		OutDebug = TEXT("Cooldown");
		return false;
	}

	if (GetCurrentMagAmmo() <= 0)
	{
		OutReason = EMosesFireGuardFailReason::NoAmmo;
		OutDebug = TEXT("NoAmmo");
		return false;
	}

	return true;
}

// ============================================================================
// [DAY3] WeaponData Resolve (존재 검증)
// ============================================================================
const UMosesWeaponData* UMosesCombatComponent::Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const
{
	OutWeaponId = GetEquippedWeaponId();
	if (!OutWeaponId.IsValid())
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return nullptr;
	}

	// ✅ 네 프로젝트 실제 API는 ResolveWeaponData()
	return Registry->ResolveWeaponData(OutWeaponId);
}

// ============================================================================
// [DAY3] Ammo Consume
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
// [DAY3] Cooldown 정책
// ============================================================================
bool UMosesCombatComponent::Server_IsFireCooldownReady(const UMosesWeaponData* /*WeaponData*/) const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double Interval = DefaultFireIntervalSec;

	const double Last =
		(CurrentSlot == 1) ? Slot1LastFireTimeSec :
		(CurrentSlot == 2) ? Slot2LastFireTimeSec :
		(CurrentSlot == 3) ? Slot3LastFireTimeSec : -9999.0;

	return (Now - Last) >= Interval;
}

void UMosesCombatComponent::Server_UpdateFireCooldownStamp()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (CurrentSlot == 1)
	{
		Slot1LastFireTimeSec = Now;
	}
	else if (CurrentSlot == 2)
	{
		Slot2LastFireTimeSec = Now;
	}
	else if (CurrentSlot == 3)
	{
		Slot3LastFireTimeSec = Now;
	}
}

// ============================================================================
// [DAY3] Hitscan + Damage + Headshot log
// ============================================================================
void UMosesCombatComponent::Server_PerformHitscanAndApplyDamage(const UMosesWeaponData* /*WeaponData*/)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
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

	const FVector Start = ViewLoc;
	const FVector End = Start + (ViewRot.Vector() * HitscanDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Moses_FireTrace), true);
	Params.AddIgnoredActor(GetOwner());

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FireTraceChannel, Params);
	if (!bHit || !Hit.GetActor())
	{
		UE_LOG(LogMosesCombat, Verbose, TEXT("[HIT][SV] Miss"));
		return;
	}

	const bool bHeadshot = (Hit.BoneName == HeadshotBoneName);
	const float AppliedDamage = DefaultDamage * (bHeadshot ? HeadshotDamageMultiplier : 1.0f);

	UE_LOG(LogMosesCombat, Log, TEXT("[HIT][SV] Victim=%s Bone=%s Headshot=%d Damage=%.1f"),
		*GetNameSafe(Hit.GetActor()), *Hit.BoneName.ToString(), bHeadshot ? 1 : 0, AppliedDamage);

	UGameplayStatics::ApplyDamage(
		Hit.GetActor(),
		AppliedDamage,
		Controller,
		GetOwner(),
		nullptr
	);
}

// ============================================================================
// Dead Hook (외부 HP 시스템이 서버 확정 시 호출)
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
	BroadcastDeadChanged(TEXT("ServerMarkDead"));
}

// ============================================================================
// RepNotifies
// ============================================================================
void UMosesCombatComponent::OnRep_CurrentSlot()
{
	BroadcastEquippedChanged(TEXT("OnRep_CurrentSlot"));
	BroadcastAmmoChanged(TEXT("OnRep_CurrentSlot"));
}

void UMosesCombatComponent::OnRep_Slot1WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot1WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot2WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot2WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot3WeaponId()
{
	BroadcastEquippedChanged(TEXT("OnRep_Slot3WeaponId"));
}

void UMosesCombatComponent::OnRep_Slot1Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot1Ammo"));
}

void UMosesCombatComponent::OnRep_Slot2Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot2Ammo"));
}

void UMosesCombatComponent::OnRep_Slot3Ammo()
{
	BroadcastAmmoChanged(TEXT("OnRep_Slot3Ammo"));
}

void UMosesCombatComponent::OnRep_IsDead()
{
	BroadcastDeadChanged(TEXT("OnRep_IsDead"));
}

// ============================================================================
// Broadcast helpers
// ============================================================================
void UMosesCombatComponent::BroadcastEquippedChanged(const TCHAR* ContextTag)
{
	const FGameplayTag WeaponId = GetEquippedWeaponId();
	OnEquippedChanged.Broadcast(CurrentSlot, WeaponId);

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][REP] %s Slot=%d Weapon=%s"),
		ContextTag, CurrentSlot, *WeaponId.ToString());
}

void UMosesCombatComponent::BroadcastAmmoChanged(const TCHAR* ContextTag)
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	OnAmmoChanged.Broadcast(Mag, Reserve);

	UE_LOG(LogMosesCombat, Log, TEXT("[AMMO][REP] %s Slot=%d Mag=%d Reserve=%d"),
		ContextTag, CurrentSlot, Mag, Reserve);
}

void UMosesCombatComponent::BroadcastDeadChanged(const TCHAR* ContextTag)
{
	OnDeadChanged.Broadcast(bIsDead);

	UE_LOG(LogMosesCombat, Log, TEXT("[DEAD][REP] %s bIsDead=%d"),
		ContextTag, bIsDead ? 1 : 0);
}

// ============================================================================
// Slot helpers
// ============================================================================
bool UMosesCombatComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 1 && SlotIndex <= 3;
}

FGameplayTag UMosesCombatComponent::GetSlotWeaponIdInternal(int32 SlotIndex) const
{
	if (SlotIndex == 1) return Slot1WeaponId;
	if (SlotIndex == 2) return Slot2WeaponId;
	if (SlotIndex == 3) return Slot3WeaponId;
	return FGameplayTag();
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

	// 아직 미초기화(0/0)라면 최소값 세팅
	if (Mag == 0 && Reserve == 0)
	{
		// Day3 최소: 무기별 탄약 정책은 Day4로 미룸
		Mag = 30;
		Reserve = 90;

		SetSlotAmmo_Internal(SlotIndex, Mag, Reserve);

		UE_LOG(LogMosesCombat, Log, TEXT("[AMMO][SV] Init Slot=%d Mag=%d Reserve=%d Weapon=%s"),
			SlotIndex, Mag, Reserve, *WeaponId.ToString());
	}
}

void UMosesCombatComponent::GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserve) const
{
	OutMag = 0;
	OutReserve = 0;

	if (SlotIndex == 1)
	{
		OutMag = Slot1MagAmmo;
		OutReserve = Slot1ReserveAmmo;
		return;
	}
	if (SlotIndex == 2)
	{
		OutMag = Slot2MagAmmo;
		OutReserve = Slot2ReserveAmmo;
		return;
	}
	if (SlotIndex == 3)
	{
		OutMag = Slot3MagAmmo;
		OutReserve = Slot3ReserveAmmo;
		return;
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

	if (SlotIndex == 1)
	{
		Slot1MagAmmo = NewMag;
		Slot1ReserveAmmo = NewReserve;
		OnRep_Slot1Ammo(); // 서버에서도 즉시 HUD 반영(RepNotify 동일 경로)
		return;
	}
	if (SlotIndex == 2)
	{
		Slot2MagAmmo = NewMag;
		Slot2ReserveAmmo = NewReserve;
		OnRep_Slot2Ammo();
		return;
	}
	if (SlotIndex == 3)
	{
		Slot3MagAmmo = NewMag;
		Slot3ReserveAmmo = NewReserve;
		OnRep_Slot3Ammo();
		return;
	}
}
