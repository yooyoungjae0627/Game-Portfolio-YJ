// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Player/Components/MosesCombatComponent.cpp
// (FULL - UPDATED / CLEAN / COMPILE-READY)
//
// ✅ 포함사항
// - 네가 올린 .cpp 내용 "전부" 포함
// - Heartbeat/AutoFire 쪽 컴파일 깨지는 멤버/이름 불일치 전부 정리
// - Owner=PlayerState 패턴에 맞게 Pawn resolve 전부 MosesCombat_Private::GetOwnerPawn(this)로 통일
// - 서버 AutoFireTick에서 Heartbeat timeout 시 강제 Stop (연사 고착 방지)
// ============================================================================

#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesCombatComponent.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/PlayerCharacter.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponData.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/Weapon/MosesGrenadeProjectile.h"
#include "UE5_Multi_Shooter/Match/GAS/MosesGameplayTags.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/MosesPlayerController.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputCoreTypes.h" // EKeys

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
// Ctor / BeginPlay / EndPlay / Replication
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
		// ✅ 안전장치: 서버에서 시작 시 연사 상태/타이머는 무조건 초기화
		bWantsToFire = false;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
			World->GetTimerManager().ClearTimer(ReloadTimerHandle);
		}

		UE_LOG(LogMosesCombat, Warning,
			TEXT("[FIRE][SV] BeginPlay Reset AutoFire/Reload Timers PS=%s"),
			*GetNameSafe(GetOwner()));

		UE_LOG(LogMosesCombat, Warning,
			TEXT("[WEAPON][SV] CombatComponent BeginPlay Owner=%s OwnerClass=%s DamageGE_SoftPath=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetOwner()->GetClass()),
			*DamageGE_SetByCaller.ToSoftObjectPath().ToString());
	}
}

void UMosesCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ✅ PIE 종료/Travel/월드 파괴 시 타이머 잔존 방지(클라 heartbeat 포함)
	StopClientFireHeldHeartbeat();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bWantsToFire = false;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
			World->GetTimerManager().ClearTimer(ReloadTimerHandle);
		}

		UE_LOG(LogMosesCombat, Warning,
			TEXT("[FIRE][SV] EndPlay Clear AutoFire/Reload Timers Reason=%d PS=%s"),
			(int32)EndPlayReason,
			*GetNameSafe(GetOwner()));
	}

	Super::EndPlay(EndPlayReason);
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
	DOREPLIFETIME(UMosesCombatComponent, Slot1ReserveMax);

	DOREPLIFETIME(UMosesCombatComponent, Slot2MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot2ReserveAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot2ReserveMax);

	DOREPLIFETIME(UMosesCombatComponent, Slot3MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot3ReserveAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot3ReserveMax);

	DOREPLIFETIME(UMosesCombatComponent, Slot4MagAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot4ReserveAmmo);
	DOREPLIFETIME(UMosesCombatComponent, Slot4ReserveMax);

	DOREPLIFETIME(UMosesCombatComponent, bIsDead);
	DOREPLIFETIME(UMosesCombatComponent, bIsReloading);

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
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Cur, Max);
	return Mag;
}

int32 UMosesCombatComponent::GetCurrentReserveAmmo() const
{
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Cur, Max);
	return Cur;
}

int32 UMosesCombatComponent::GetCurrentReserveMax() const
{
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Cur, Max);
	return Max;
}

// ============================================================================
// Default init / loadout (Server)
// ============================================================================

void UMosesCombatComponent::ServerInitDefaultSlots_4(
	const FGameplayTag& InSlot1,
	const FGameplayTag& InSlot2,
	const FGameplayTag& InSlot3,
	const FGameplayTag& InSlot4)
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

// ============================================================================
// Default 지급: Rifle 30/90 + ReserveMax=90
// ============================================================================

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

	SetSlotAmmo_Internal(1, 30, 90, 90);

	if (CurrentSlot == 1)
	{
		OnRep_CurrentSlot();
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] DefaultRifleAmmo Granted Slot=1 Mag=30 Reserve=90/90 Weapon=%s"),
		*Slot1WeaponId.ToString());
}

// ============================================================================
// [AMMO_PICKUP] Ammo Farming - Server Authority ONLY
// ============================================================================

void UMosesCombatComponent::ServerAddAmmoByType(EMosesAmmoType AmmoType, int32 ReserveMaxDelta, int32 ReserveFillDelta)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UMosesWeaponRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UMosesWeaponRegistrySubsystem>() : nullptr;

	if (!Registry)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] AddAmmoByType FAIL (NoRegistry) PS=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	bool bAppliedAny = false;

	for (int32 Slot = 1; Slot <= 4; ++Slot)
	{
		const FGameplayTag WeaponId = GetWeaponIdForSlot(Slot);
		if (!WeaponId.IsValid())
		{
			continue;
		}

		const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
		if (!Data)
		{
			continue;
		}

		if (Data->AmmoType != AmmoType)
		{
			continue;
		}

		int32 Mag = 0, Cur = 0, Max = 0;
		GetSlotAmmo_Internal(Slot, Mag, Cur, Max);

		const int32 OldCur = Cur;
		const int32 OldMax = Max;

		Max = FMath::Max(0, Max + FMath::Max(0, ReserveMaxDelta));
		Cur = FMath::Clamp(Cur + FMath::Max(0, ReserveFillDelta), 0, Max);

		SetSlotAmmo_Internal(Slot, Mag, Cur, Max);

		UE_LOG(LogMosesWeapon, Warning,
			TEXT("[AMMO][SV] PickupApplied Slot=%d Weapon=%s Type=%d Reserve %d/%d -> %d/%d (DeltaMax=%d Fill=%d) PS=%s"),
			Slot,
			*WeaponId.ToString(),
			(int32)AmmoType,
			OldCur, OldMax,
			Cur, Max,
			ReserveMaxDelta,
			ReserveFillDelta,
			*GetNameSafe(GetOwner()));

		bAppliedAny = true;

		if (CurrentSlot == Slot)
		{
			BroadcastAmmoChanged(TEXT("ServerAddAmmoByType(CurrentSlot)"));
		}

		BroadcastSlotsStateChanged(Slot, TEXT("ServerAddAmmoByType"));
	}

	if (!bAppliedAny)
	{
		UE_LOG(LogMosesWeapon, Warning,
			TEXT("[AMMO][SV] Pickup FAIL (No slot using AmmoType=%d) PS=%s"),
			(int32)AmmoType,
			*GetNameSafe(GetOwner()));
	}
}

// ============================================================================
// Grant/Pickup API (Server only)
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

	if (GetSlotWeaponIdInternal(ClampedSlot) == WeaponId)
	{
		UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][SV] GrantWeaponToSlot SKIP (SameWeapon) Slot=%d Weapon=%s PS=%s"),
			ClampedSlot, *WeaponId.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	Server_SetSlotWeaponId_Internal(ClampedSlot, WeaponId);

	if (bInitializeAmmoIfEmpty)
	{
		Server_EnsureAmmoInitializedForSlot(ClampedSlot, WeaponId);
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] GrantWeaponToSlot OK Slot=%d Weapon=%s (InitAmmo=%d) PS=%s"),
		ClampedSlot, *WeaponId.ToString(), bInitializeAmmoIfEmpty ? 1 : 0, *GetNameSafe(GetOwner()));

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

	// ✅ 스왑 시작 시 연사 끊기
	bWantsToFire = false;
	StopAutoFire_Server();

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

	LastSwapFromSlot = OldSlot;
	LastSwapToSlot = NewSlot;
	SwapSerial++;

	CurrentSlot = NewSlot;

	Server_EnsureAmmoInitializedForSlot(CurrentSlot, NewWeaponId);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Swap OK Slot %d -> %d Weapon %s -> %s Serial=%d"),
		OldSlot, CurrentSlot, *OldWeaponId.ToString(), *NewWeaponId.ToString(), SwapSerial);

	OnRep_SwapSerial();
	OnRep_CurrentSlot();

	BroadcastEquippedChanged(TEXT("ServerEquipSlot"));
	BroadcastAmmoChanged(TEXT("ServerEquipSlot"));
}

// ============================================================================
// Fire API (단발)
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
		return;
	}

	FGameplayTag ApprovedWeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(ApprovedWeaponId);
	if (!WeaponData)
	{
		return;
	}

	if (!Server_IsFireCooldownReady(WeaponData))
	{
		return;
	}

	// ✅ 여기서부터가 "승인 후" 구간
	Server_UpdateFireCooldownStamp();

	// ✅ [여기에 삽입]
	const bool bAmmoCostApplied = Server_ApplyAmmoCostToSelf_GAS(1.0f, WeaponData);
	if (!bAmmoCostApplied)
	{
		Server_ConsumeAmmo_OnApprovedFire(WeaponData);
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Fire Weapon=%s Slot=%d Mag=%d Reserve=%d"),
		*ApprovedWeaponId.ToString(), CurrentSlot, GetCurrentMagAmmo(), GetCurrentReserveAmmo());

	Server_PerformFireAndApplyDamage(WeaponData);
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
		UE_LOG(LogMosesWeapon, Warning, TEXT("[RELOAD][SV] REJECT Dead=%d Reloading=%d Slot=%d"),
			bIsDead ? 1 : 0, bIsReloading ? 1 : 0, CurrentSlot);
		return;
	}

	FGameplayTag WeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(WeaponId);
	if (!WeaponData)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[RELOAD][SV] REJECT NoWeaponData Slot=%d Weapon=%s"),
			CurrentSlot, *WeaponId.ToString());
		return;
	}

	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	UE_LOG(LogMosesWeapon, Warning,
		TEXT("[RELOAD][SV] TRY Slot=%d Weapon=%s Mag=%d Reserve=%d MagSize=%d"),
		CurrentSlot,
		*WeaponId.ToString(),
		Mag,
		Reserve,
		WeaponData->MagSize);

	if (Reserve <= 0 || Mag >= WeaponData->MagSize)
	{
		UE_LOG(LogMosesWeapon, Warning,
			TEXT("[RELOAD][SV] REJECT Reserve=%d Mag=%d MagSize=%d"),
			Reserve, Mag, WeaponData->MagSize);
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ✅ 사망/리로드 시작 시 연사 끊기
	bWantsToFire = false;
	StopAutoFire_Server();

	bIsReloading = true;
	OnRep_IsReloading();

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] ReloadStart Slot=%d Weapon=%s Sec=%.2f"),
		CurrentSlot, *GetEquippedWeaponId().ToString(), WeaponData->ReloadSeconds);

	World->GetTimerManager().SetTimer(
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

void UMosesCombatComponent::Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData)
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	const int32 OldMag = Mag;

	Mag = FMath::Max(Mag - 1, 0);
	SetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	BroadcastAmmoChanged(TEXT("Server_ConsumeAmmo_OnApprovedFire"));

	if (bAutoReloadOnEmpty
		&& !bIsDead
		&& !bIsReloading
		&& OldMag > 0
		&& Mag == 0
		&& Reserve > 0)
	{
		UE_LOG(LogMosesWeapon, Warning,
			TEXT("[RELOAD][SV][AUTO] Triggered Slot=%d OldMag=%d NewMag=%d Reserve=%d PS=%s"),
			CurrentSlot, OldMag, Mag, Reserve, *GetNameSafe(GetOwner()));

		ServerReload_Implementation();
	}
}

// ============================================================================
// Cooldown
// ============================================================================

float UMosesCombatComponent::Server_GetFireIntervalSec_FromWeaponData(const UMosesWeaponData* WeaponData) const
{
	float Interval = WeaponData ? WeaponData->FireIntervalSec : DefaultFireIntervalSec;

	const float MinInterval = 0.03f;
	Interval = FMath::Max(MinInterval, Interval);

	return Interval;
}

bool UMosesCombatComponent::Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const
{
	if (!GetWorld())
	{
		return false;
	}

	const int32 SlotIndex = FMath::Clamp(CurrentSlot, 1, 4) - 1;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Interval = Server_GetFireIntervalSec_FromWeaponData(WeaponData);

	return (Now - SlotLastFireTimeSec[SlotIndex]) >= Interval;
}

void UMosesCombatComponent::Server_UpdateFireCooldownStamp()
{
	if (!GetWorld())
	{
		return;
	}

	const int32 SlotIndex = FMath::Clamp(CurrentSlot, 1, 4) - 1;
	SlotLastFireTimeSec[SlotIndex] = GetWorld()->GetTimeSeconds();
}

// ============================================================================
// Spread / Fire Core
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

	UWorld* World = GetWorld();
	if (!World)
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

	FVector MuzzleStart = OwnerPawn->GetPawnViewLocation();
	bool bGotMuzzle = false;

	if (WeaponData)
	{
		const FName MuzzleSocketName = WeaponData->MuzzleSocketName;

		if (!MuzzleSocketName.IsNone())
		{
			TInlineComponentArray<USkeletalMeshComponent*> SkelComps;
			OwnerPawn->GetComponents<USkeletalMeshComponent>(SkelComps);

			for (USkeletalMeshComponent* Comp : SkelComps)
			{
				if (!Comp)
				{
					continue;
				}

				if (Comp->GetFName() == TEXT("WeaponMesh_Hand"))
				{
					if (Comp->DoesSocketExist(MuzzleSocketName))
					{
						MuzzleStart = Comp->GetSocketLocation(MuzzleSocketName);
						bGotMuzzle = true;
					}
					break;
				}
			}
		}
	}

	// Projectile weapon
	if (WeaponData && WeaponData->bIsProjectileWeapon)
	{
		Server_SpawnGrenadeProjectile(WeaponData, MuzzleStart, SpreadDir, Controller, OwnerPawn);
		return;
	}

	const FVector CamStart = ViewLoc;
	const FVector CamEnd = CamStart + (SpreadDir * HitscanDistance);

	FCollisionQueryParams CamParams(SCENE_QUERY_STAT(Moses_FireTrace_Cam), true);
	CamParams.AddIgnoredActor(OwnerPawn);

	if (USkeletalMeshComponent* MeshComp = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		CamParams.AddIgnoredComponent(MeshComp);
	}

	FHitResult CamHit;
	const bool bCamHit = World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, FireTraceChannel, CamParams);
	const FVector CamImpact = (bCamHit ? (CamHit.Location.IsNearlyZero() ? CamHit.ImpactPoint : CamHit.Location) : CamEnd);
	const FVector AimPoint = CamImpact;

	FVector MuzzleEnd = AimPoint;

	const float DistMS = FVector::Dist(MuzzleStart, MuzzleEnd);
	if (DistMS < 10.0f)
	{
		MuzzleEnd = MuzzleStart + (SpreadDir * 10.0f);
	}

	FCollisionQueryParams MuzzleParams(SCENE_QUERY_STAT(Moses_FireTrace_Muzzle), true);
	MuzzleParams.AddIgnoredActor(OwnerPawn);

	{
		TInlineComponentArray<USkeletalMeshComponent*> SkelComps;
		OwnerPawn->GetComponents<USkeletalMeshComponent>(SkelComps);
		for (USkeletalMeshComponent* Comp : SkelComps)
		{
			if (!Comp)
			{
				continue;
			}

			if (Comp == OwnerPawn->FindComponentByClass<USkeletalMeshComponent>() || Comp->GetFName() == TEXT("WeaponMesh_Hand"))
			{
				MuzzleParams.AddIgnoredComponent(Comp);
			}
		}
	}

	FHitResult FinalHit;
	const bool bFinalHit = World->LineTraceSingleByChannel(FinalHit, MuzzleStart, MuzzleEnd, FireTraceChannel, MuzzleParams);

	const FVector FinalImpact = (bFinalHit ? (FinalHit.Location.IsNearlyZero() ? FinalHit.ImpactPoint : FinalHit.Location) : MuzzleEnd);

	if (bServerTraceDebugDraw)
	{
		Multicast_DrawFireTraceDebug(
			CamStart, CamEnd,
			bCamHit, CamImpact,
			MuzzleStart, MuzzleEnd,
			bFinalHit, FinalImpact);
	}

	if (!bFinalHit || !FinalHit.GetActor())
	{
		return;
	}

	// ---------------------------------------------------------------------
	// Headshot 판정 강화 (좀비 HitBox 대응)
	// ---------------------------------------------------------------------
	bool bHeadshot = false;

	UPrimitiveComponent* HitComp = Cast<UPrimitiveComponent>(FinalHit.GetComponent());
	const FString HitCompName = HitComp ? HitComp->GetName() : TEXT("None");

	if (HitComp && HitComp->ComponentHasTag(TEXT("HitZone.Head")))
	{
		bHeadshot = true;
	}

	if (!bHeadshot)
	{
		if (const APlayerCharacter* VictimPlayer = Cast<APlayerCharacter>(FinalHit.GetActor()))
		{
			if (VictimPlayer->GetHeadHitBox() && FinalHit.GetComponent() == VictimPlayer->GetHeadHitBox())
			{
				bHeadshot = true;
			}
		}
	}

	if (!bHeadshot)
	{
		if (HitComp && HitCompName.Contains(TEXT("HeadHitBox")))
		{
			bHeadshot = true;
		}
	}

	if (!bHeadshot)
	{
		if (!FinalHit.BoneName.IsNone())
		{
			const FString BoneLower = FinalHit.BoneName.ToString().ToLower();
			bHeadshot = (FinalHit.BoneName == HeadshotBoneName) || BoneLower.Contains(TEXT("head"));
		}
	}

	const float BaseDamage = WeaponData ? WeaponData->Damage : DefaultDamage;

	const bool bIsZombie = Server_IsZombieTarget(FinalHit.GetActor());

	float AppliedDamage = BaseDamage;
	if (bHeadshot)
	{
		if (bIsZombie)
		{
			AppliedDamage = 99999.0f;
		}
		else
		{
			AppliedDamage = BaseDamage * HeadshotDamageMultiplier;
		}
	}

	UE_LOG(LogMosesCombat, Warning, TEXT("[HIT][SV] Victim=%s Comp=%s Bone=%s Headshot=%d IsZombie=%d Damage=%.1f"),
		*GetNameSafe(FinalHit.GetActor()),
		*GetNameSafe(FinalHit.GetComponent()),
		FinalHit.BoneName.IsNone() ? TEXT("None") : *FinalHit.BoneName.ToString(),
		bHeadshot ? 1 : 0,
		bIsZombie ? 1 : 0,
		AppliedDamage);

	const bool bAppliedByGAS = Server_ApplyDamageToTarget_GAS(
		FinalHit.GetActor(),
		AppliedDamage,
		Controller,
		OwnerPawn,
		WeaponData,
		FinalHit);

	if (!bAppliedByGAS)
	{
		UGameplayStatics::ApplyDamage(FinalHit.GetActor(), AppliedDamage, Controller, OwnerPawn, nullptr);
	}
}

void UMosesCombatComponent::Server_SpawnGrenadeProjectile(
	const UMosesWeaponData* WeaponData,
	const FVector& SpawnLoc,
	const FVector& FireDir,
	AController* InstigatorController,
	APawn* OwnerPawn)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!WeaponData || !OwnerPawn)
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[GRENADE][SV] Spawn FAIL (Invalid WeaponData or OwnerPawn)"));
		return;
	}

	TSubclassOf<AMosesGrenadeProjectile> ProjectileClass = WeaponData->ProjectileClass;
	if (!ProjectileClass)
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[GRENADE][SV] Spawn FAIL (WeaponData ProjectileClass is null) Weapon=%s"),
			*WeaponData->WeaponId.ToString());
		return;
	}

	AMosesPlayerState* PS = MosesCombat_Private::GetOwnerPS(this);
	if (!PS)
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[GRENADE][SV] Spawn FAIL (No PlayerState owner)"));
		return;
	}

	UAbilitySystemComponent* SourceASC = PS->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[GRENADE][SV] Spawn FAIL (No Source ASC) PS=%s"),
			*GetNameSafe(PS));
		return;
	}

	TSubclassOf<UGameplayEffect> GEClass = DamageGE_SetByCaller.LoadSynchronous();
	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Warning,
			TEXT("[GRENADE][SV] DamageGE missing -> FallbackOnly. SoftPath=%s"),
			*DamageGE_SetByCaller.ToSoftObjectPath().ToString());
	}

	FActorSpawnParameters Params;
	Params.Owner = OwnerPawn;
	Params.Instigator = OwnerPawn;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FRotator SpawnRot = FireDir.Rotation();

	AMosesGrenadeProjectile* Projectile =
		GetWorld()->SpawnActor<AMosesGrenadeProjectile>(ProjectileClass, SpawnLoc, SpawnRot, Params);

	if (!Projectile)
	{
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[GRENADE][SV] Spawn FAIL (SpawnActor returned null)"));
		return;
	}

	const float ExplodeDamage =
		(WeaponData->ExplosionDamageOverride > 0.0f)
		? WeaponData->ExplosionDamageOverride
		: WeaponData->Damage;

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
		TEXT("[GRENADE][SV] Spawn OK Weapon=%s Loc=%s Speed=%.1f Radius=%.1f Damage=%.1f GE=%s"),
		*WeaponData->WeaponId.ToString(),
		*SpawnLoc.ToCompactString(),
		WeaponData->ProjectileSpeed,
		WeaponData->ExplosionRadius,
		ExplodeDamage,
		GEClass ? *GetNameSafe(GEClass.Get()) : TEXT("None(FallbackOnly)"));
}

bool UMosesCombatComponent::Server_ApplyDamageToTarget_GAS(
	AActor* TargetActor,
	float Damage,
	AController* InstigatorController,
	AActor* DamageCauser,
	const UMosesWeaponData* WeaponData,
	const FHitResult& Hit) const
{
	if (APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		if (AMosesPlayerState* TargetPS = TargetPawn->GetPlayerState<AMosesPlayerState>())
		{
			if (TargetPS->IsDead())
			{
				UE_LOG(LogMosesGAS, Verbose,
					TEXT("[GAS][SV] APPLY SKIP (TargetAlreadyDead) TargetPawn=%s TargetPS=%s"),
					*GetNameSafe(TargetPawn),
					*GetNameSafe(TargetPS));
				return false;
			}
		}
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!TargetActor)
	{
		return false;
	}

	AMosesPlayerState* SourcePS = Cast<AMosesPlayerState>(GetOwner());
	if (!SourcePS)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = SourcePS->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = nullptr;
	AActor* ResolvedTargetOwnerForLog = TargetActor;

	if (IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(TargetActor))
	{
		TargetASC = TargetASI->GetAbilitySystemComponent();
	}
	else if (APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		if (AMosesPlayerState* TargetPS = TargetPawn->GetPlayerState<AMosesPlayerState>())
		{
			TargetASC = TargetPS->GetAbilitySystemComponent();
			ResolvedTargetOwnerForLog = TargetPS;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	if (!TargetASC)
	{
		return false;
	}

	TSubclassOf<UGameplayEffect> GEClass = nullptr;

	const bool bZombieTarget = Server_IsZombieTarget(TargetActor);

	if (bZombieTarget)
	{
		GEClass = SourcePS->GetDamageGE_Zombie_SetByCaller();
	}
	else
	{
		GEClass = SourcePS->GetDamageGE_Player_SetByCaller();
	}

	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Error,
			TEXT("[GAS][SV] APPLY FAIL (NoDamageGE_ByPolicy) Target=%s IsZombie=%d PS=%s"),
			*GetNameSafe(TargetActor),
			bZombieTarget ? 1 : 0,
			*GetNameSafe(SourcePS));
		return false;
	}

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();

	AActor* InstigatorActor = InstigatorController ? InstigatorController->GetPawn() : nullptr;
	if (!InstigatorActor)
	{
		InstigatorActor = DamageCauser;
	}

	Ctx.AddInstigator(InstigatorActor, DamageCauser);
	Ctx.AddSourceObject(this);
	Ctx.AddSourceObject(WeaponData);
	Ctx.AddHitResult(Hit, true);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEClass, 1.0f, Ctx);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	// Headshot 판정(동일 로직 유지)
	bool bIsHeadshot = false;

	UPrimitiveComponent* HitComp = Cast<UPrimitiveComponent>(Hit.GetComponent());
	const FString HitCompName = HitComp ? HitComp->GetName() : TEXT("None");

	if (HitComp && HitComp->ComponentHasTag(TEXT("HitZone.Head")))
	{
		bIsHeadshot = true;
	}

	if (!bIsHeadshot)
	{
		if (const APlayerCharacter* VictimPlayer = Cast<APlayerCharacter>(TargetActor))
		{
			if (VictimPlayer->GetHeadHitBox() && Hit.GetComponent() == VictimPlayer->GetHeadHitBox())
			{
				bIsHeadshot = true;
			}
		}
	}

	if (!bIsHeadshot)
	{
		if (HitComp && HitCompName.Contains(TEXT("HeadHitBox")))
		{
			bIsHeadshot = true;
		}
	}

	if (!bIsHeadshot)
	{
		if (!Hit.BoneName.IsNone())
		{
			const FString BoneLower = Hit.BoneName.ToString().ToLower();
			bIsHeadshot = (Hit.BoneName == HeadshotBoneName) || BoneLower.Contains(TEXT("head"));
		}
	}

	if (bIsHeadshot)
	{
		SpecHandle.Data->AddDynamicAssetTag(FMosesGameplayTags::Get().Hit_Headshot);
	}

	float FinalDamageForSetByCaller = FMath::Abs(Damage);
	if (bZombieTarget && bIsHeadshot)
	{
		FinalDamageForSetByCaller = 99999.0f;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Damage, FinalDamageForSetByCaller);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogMosesGAS, Warning,
		TEXT("[GAS][SV] APPLY OK TargetActor=%s ResolvedOwner=%s Damage=%.1f Weapon=%s GE=%s Bone=%s Headshot=%d IsZombie=%d HitComp=%s"),
		*GetNameSafe(TargetActor),
		*GetNameSafe(ResolvedTargetOwnerForLog),
		FinalDamageForSetByCaller,
		WeaponData ? *WeaponData->WeaponId.ToString() : TEXT("None"),
		*GetNameSafe(GEClass.Get()),
		Hit.BoneName.IsNone() ? TEXT("None") : *Hit.BoneName.ToString(),
		bIsHeadshot ? 1 : 0,
		bZombieTarget ? 1 : 0,
		*HitCompName);

	if (bIsHeadshot && InstigatorController)
	{
		if (AMosesPlayerController* MPC = Cast<AMosesPlayerController>(InstigatorController))
		{
			MPC->Client_ShowHeadshotToast_OwnerOnly(FText::FromString(TEXT("헤드샷!")), 0.8f);
		}
	}

	return true;
}

// ============================================================================
// Cosmetic propagation
// ============================================================================

void UMosesCombatComponent::Server_PropagateFireCosmetics(FGameplayTag ApprovedWeaponId)
{
	APlayerCharacter* PlayerChar = MosesCombat_Private::GetOwnerPlayerCharacter(this);
	if (!PlayerChar)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[FIRE][SV] PropagateCosmetic FAIL (No Pawn) PS=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[FIRE][SV] PropagateCosmetic OK ShooterPawn=%s Weapon=%s"),
		*GetNameSafe(PlayerChar),
		*ApprovedWeaponId.ToString());

	PlayerChar->Multicast_PlayFireMontage(ApprovedWeaponId);
}

// ============================================================================
// Dead / Respawn
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

	bWantsToFire = false;
	StopAutoFire_Server();

	bIsReloading = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	OnRep_IsDead();
	OnRep_IsReloading();

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[DEAD][SV][CC] MarkDead bIsDead=1 PS=%s PS_Pawn=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(MosesCombat_Private::GetOwnerPawn(this)));
}

void UMosesCombatComponent::ServerClearDeadAfterRespawn()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bWantsToFire = false;
	StopAutoFire_Server();

	const bool bOldDead = bIsDead;
	const bool bOldReload = bIsReloading;

	bIsDead = false;
	bIsReloading = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	if (bOldDead)
	{
		OnRep_IsDead();
	}
	if (bOldReload)
	{
		OnRep_IsReloading();
	}

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[RESPAWN][SV][CC] ClearDead DONE Dead %d->0 Reload %d->0 (AutoFire cleared) PS=%s Pawn=%s"),
		bOldDead ? 1 : 0,
		bOldReload ? 1 : 0,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(MosesCombat_Private::GetOwnerPawn(this)));
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

void UMosesCombatComponent::OnRep_IsDead()
{
	BroadcastDeadChanged(TEXT("OnRep_IsDead"));
}

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
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Cur, Max);

	OnAmmoChanged.Broadcast(Mag, Cur);
	OnAmmoChangedEx.Broadcast(Mag, Cur, Max);

	UE_LOG(LogMosesCombat, Warning, TEXT("[AMMO][REP] %s Slot=%d Mag=%d Reserve=%d/%d"),
		ContextTag ? ContextTag : TEXT("None"),
		CurrentSlot,
		Mag,
		Cur,
		Max);
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

	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Cur, Max);

	if (Mag > 0 || Cur > 0 || Max > 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UMosesWeaponRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UMosesWeaponRegistrySubsystem>() : nullptr;

	const UMosesWeaponData* Data = Registry ? Registry->ResolveWeaponData(WeaponId) : nullptr;
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] EnsureAmmo FAIL (NoWeaponData) Slot=%d Weapon=%s PS=%s"),
			SlotIndex, *WeaponId.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	const int32 InitMag = FMath::Max(0, Data->MagSize);
	const int32 InitMax = FMath::Max(0, Data->MaxReserve);
	const int32 InitCur = InitMax;

	SetSlotAmmo_Internal(SlotIndex, InitMag, InitCur, InitMax);

	UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] EnsureAmmo INIT Slot=%d Weapon=%s Ammo Mag=%d Reserve=%d/%d PS=%s"),
		SlotIndex, *WeaponId.ToString(), InitMag, InitCur, InitMax, *GetNameSafe(GetOwner()));

	if (CurrentSlot == SlotIndex)
	{
		BroadcastAmmoChanged(TEXT("Server_EnsureAmmoInitializedForSlot(CurrentSlot)"));
	}
}

// ============================================================================
// Ammo helpers (ReserveMax 포함)
// ============================================================================

void UMosesCombatComponent::GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur, int32& OutReserveMax) const
{
	OutMag = 0;
	OutReserveCur = 0;
	OutReserveMax = 0;

	switch (SlotIndex)
	{
	case 1:
		OutMag = Slot1MagAmmo;
		OutReserveCur = Slot1ReserveAmmo;
		OutReserveMax = Slot1ReserveMax;
		return;

	case 2:
		OutMag = Slot2MagAmmo;
		OutReserveCur = Slot2ReserveAmmo;
		OutReserveMax = Slot2ReserveMax;
		return;

	case 3:
		OutMag = Slot3MagAmmo;
		OutReserveCur = Slot3ReserveAmmo;
		OutReserveMax = Slot3ReserveMax;
		return;

	case 4:
		OutMag = Slot4MagAmmo;
		OutReserveCur = Slot4ReserveAmmo;
		OutReserveMax = Slot4ReserveMax;
		return;

	default:
		return;
	}
}

void UMosesCombatComponent::SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur, int32 NewReserveMax)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	NewMag = FMath::Max(NewMag, 0);
	NewReserveMax = FMath::Max(NewReserveMax, 0);
	NewReserveCur = FMath::Clamp(NewReserveCur, 0, NewReserveMax);

	switch (SlotIndex)
	{
	case 1:
		Slot1MagAmmo = NewMag;
		Slot1ReserveMax = NewReserveMax;
		Slot1ReserveAmmo = NewReserveCur;
		OnRep_Slot1Ammo();
		return;

	case 2:
		Slot2MagAmmo = NewMag;
		Slot2ReserveMax = NewReserveMax;
		Slot2ReserveAmmo = NewReserveCur;
		OnRep_Slot2Ammo();
		return;

	case 3:
		Slot3MagAmmo = NewMag;
		Slot3ReserveMax = NewReserveMax;
		Slot3ReserveAmmo = NewReserveCur;
		OnRep_Slot3Ammo();
		return;

	case 4:
		Slot4MagAmmo = NewMag;
		Slot4ReserveMax = NewReserveMax;
		Slot4ReserveAmmo = NewReserveCur;
		OnRep_Slot4Ammo();
		return;

	default:
		return;
	}
}

void UMosesCombatComponent::GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserveCur) const
{
	int32 DummyMax = 0;
	GetSlotAmmo_Internal(SlotIndex, OutMag, OutReserveCur, DummyMax);
}

void UMosesCombatComponent::SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserveCur)
{
	int32 OldMag = 0;
	int32 OldCur = 0;
	int32 OldMax = 0;
	GetSlotAmmo_Internal(SlotIndex, OldMag, OldCur, OldMax);

	SetSlotAmmo_Internal(SlotIndex, NewMag, NewReserveCur, OldMax);
}

int32 UMosesCombatComponent::GetMagAmmoForSlot(int32 SlotIndex) const
{
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Cur, Max);
	return Mag;
}

int32 UMosesCombatComponent::GetReserveAmmoForSlot(int32 SlotIndex) const
{
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Cur, Max);
	return Cur;
}

int32 UMosesCombatComponent::GetReserveMaxForSlot(int32 SlotIndex) const
{
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Cur, Max);
	return Max;
}

void UMosesCombatComponent::BroadcastSlotsStateChanged(int32 ChangedSlotOr0ForAll, const TCHAR* ContextTag)
{
	OnSlotsStateChanged.Broadcast(ChangedSlotOr0ForAll);

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][HUD] SlotsStateChanged Slot=%d Ctx=%s PS=%s"),
		ChangedSlotOr0ForAll,
		ContextTag ? ContextTag : TEXT("None"),
		*GetNameSafe(GetOwner()));
}

// ============================================================================
// AddAmmoByTag
// ============================================================================

void UMosesCombatComponent::ServerAddAmmoByTag(FGameplayTag AmmoTypeId, int32 ReserveMaxDelta, int32 ReserveFillDelta)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!AmmoTypeId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] AddAmmoByTag FAIL (AmmoTypeId invalid) PS=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	EMosesAmmoType AmmoType = EMosesAmmoType::Rifle;

	const FString TagStr = AmmoTypeId.ToString();

	if (TagStr.Contains(TEXT("Ammo.Rifle"), ESearchCase::IgnoreCase))
	{
		AmmoType = EMosesAmmoType::Rifle;
	}
	else if (TagStr.Contains(TEXT("Ammo.Sniper"), ESearchCase::IgnoreCase))
	{
		AmmoType = EMosesAmmoType::Sniper;
	}
	else if (TagStr.Contains(TEXT("Ammo.Grenade"), ESearchCase::IgnoreCase))
	{
		AmmoType = EMosesAmmoType::Grenade;
	}
	else
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] AddAmmoByTag FAIL (Unknown Tag=%s) PS=%s"),
			*AmmoTypeId.ToString(),
			*GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[AMMO][SV] AddAmmoByTag -> AddAmmoByType Tag=%s Type=%d"),
		*AmmoTypeId.ToString(),
		(int32)AmmoType);

	ServerAddAmmoByType(AmmoType, ReserveMaxDelta, ReserveFillDelta);
}

// ============================================================================
// Trace Debug (Server -> All Clients)
// ============================================================================

void UMosesCombatComponent::Multicast_DrawFireTraceDebug_Implementation(
	const FVector& CamStart,
	const FVector& CamEnd,
	bool bCamHit,
	const FVector& CamImpact,
	const FVector& MuzzleStart,
	const FVector& MuzzleEnd,
	bool bFinalHit,
	const FVector& FinalImpact)
{
#if ENABLE_DRAW_DEBUG
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!World->IsGameWorld())
	{
		return;
	}

	const float Life = FMath::Max(0.01f, ServerTraceDebugDrawTime);

	DrawDebugLine(World, CamStart, CamEnd, FColor::Cyan, false, Life, 0, 1.5f);
	if (bCamHit)
	{
		DrawDebugPoint(World, CamImpact, 10.f, FColor::Cyan, false, Life);
	}

	DrawDebugLine(World, MuzzleStart, MuzzleEnd, bFinalHit ? FColor::Green : FColor::Red, false, Life, 0, 2.5f);
	if (bFinalHit)
	{
		DrawDebugPoint(World, FinalImpact, 14.f, FColor::Green, false, Life);
	}
#endif
}

bool UMosesCombatComponent::Server_IsZombieTarget(const AActor* TargetActor) const
{
	return TargetActor && TargetActor->IsA(AMosesZombieCharacter::StaticClass());
}

// ============================================================================
// AutoFire (Server) + Heartbeat (Client -> Server)
// ============================================================================

void UMosesCombatComponent::RequestStartFire()
{
	UE_LOG(LogTemp, Warning, TEXT("[CC][CL] RequestStartFire"));

	// ✅ 로컬(클라)에서 Heartbeat 시작
	StartClientFireHeldHeartbeat();

	// ✅ 서버에 연사 시작 요청
	ServerStartFire();
}

void UMosesCombatComponent::RequestStopFire()
{
	UE_LOG(LogTemp, Warning, TEXT("[CC][CL] RequestStopFire"));

	StopClientFireHeldHeartbeat();
	ServerStopFire();
}

void UMosesCombatComponent::ServerStartFire_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// ✅ 새 연사 세션 토큰 발급 (Stop 이후 남은 tick 무력화)
	++FireRequestToken;
	ActiveFireToken = FireRequestToken;

	// 이미 연사 의도 상태라도 “타이머가 꺼져있을 수 있음” -> 아래에서 재보장
	bWantsToFire = true;

	// ✅ 시작 시점 기준점(클라 heartbeat 지연 대비)
	if (UWorld* World = GetWorld())
	{
		LastFireHeldHeartbeatSec = World->GetTimeSeconds();
	}

	// ✅ 현재 무기 interval 캐시
	FGameplayTag WeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(WeaponId);
	CachedAutoFireIntervalSec = Server_GetFireIntervalSec_FromWeaponData(WeaponData);

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[FIRE][SV] StartFire Wants=1 Token=%u Interval=%.3f PS=%s"),
		ActiveFireToken,
		CachedAutoFireIntervalSec,
		*GetNameSafe(GetOwner()));

	// ✅ 즉발 1발
	AutoFireTick_Server();

	// ✅ 반복 타이머 시작(weapon interval)
	StartAutoFire_Server(CachedAutoFireIntervalSec);
}

void UMosesCombatComponent::ServerStopFire_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bWantsToFire = false;

	// ✅ 토큰 갱신: 이미 큐에 들어간 tick도 무력화
	++FireRequestToken;

	StopAutoFire_Server();

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[FIRE][SV] StopFire Wants=0 NewToken=%u PS=%s"),
		FireRequestToken,
		*GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::Server_UpdateFireHeldHeartbeat_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		LastFireHeldHeartbeatSec = World->GetTimeSeconds();
	}
}

void UMosesCombatComponent::StartAutoFire_Server(float IntervalSec)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 혹시 남아있는게 있으면 먼저 정리
	World->GetTimerManager().ClearTimer(AutoFireTimerHandle);

	const float TickRate = FMath::Max(0.01f, IntervalSec);

	World->GetTimerManager().SetTimer(
		AutoFireTimerHandle,
		this,
		&ThisClass::AutoFireTick_Server,
		TickRate,
		true,
		TickRate);

	UE_LOG(LogMosesCombat, Verbose,
		TEXT("[FIRE][SV] AutoFireTimer START Rate=%.3f Token=%u PS=%s"),
		TickRate, ActiveFireToken, *GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::StopAutoFire_Server()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}

	UE_LOG(LogMosesCombat, Verbose,
		TEXT("[FIRE][SV] AutoFireTimer STOP PS=%s"),
		*GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::AutoFireTick_Server()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// ✅ Stop 이후 큐에 남은 tick 무력화
	const uint32 TickToken = ActiveFireToken;
	if (TickToken != FireRequestToken)
	{
		UE_LOG(LogMosesCombat, Verbose,
			TEXT("[FIRE][SV] AutoFireTick IGNORED by Token Tick=%u Cur=%u PS=%s"),
			TickToken, FireRequestToken, *GetNameSafe(GetOwner()));
		return;
	}

	if (!bWantsToFire)
	{
		StopAutoFire_Server();
		return;
	}

	// ✅ Heartbeat timeout이면 연사 고착 강제 종료
	if (UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		if ((Now - LastFireHeldHeartbeatSec) > FireHeldHeartbeatTimeoutSec)
		{
			UE_LOG(LogMosesCombat, Warning,
				TEXT("[FIRE][SV] AutoFire STOP by HeartbeatTimeout Now=%.3f Last=%.3f Timeout=%.3f PS=%s"),
				Now, LastFireHeldHeartbeatSec, FireHeldHeartbeatTimeoutSec, *GetNameSafe(GetOwner()));

			bWantsToFire = false;
			++FireRequestToken; // 잔여 tick 무력화
			StopAutoFire_Server();
			return;
		}
	}

	EMosesFireGuardFailReason Reason = EMosesFireGuardFailReason::None;
	FString Debug;
	if (!Server_CanFire(Reason, Debug))
	{
		UE_LOG(LogMosesCombat, Verbose,
			TEXT("[FIRE][SV] AutoFire STOP by Guard Reason=%d Debug=%s PS=%s"),
			(int32)Reason, *Debug, *GetNameSafe(GetOwner()));

		bWantsToFire = false;
		++FireRequestToken;
		StopAutoFire_Server();
		return;
	}

	// ✅ 서버면 RPC(ServerFire)로 다시 돌지 말고 Implementation 직접 호출
	ServerFire_Implementation();
}

// ============================================================================
// Client Heartbeat (Fail-safe)
// ============================================================================

void UMosesCombatComponent::StartClientFireHeldHeartbeat()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APawn* Pawn = MosesCombat_Private::GetOwnerPawn(this);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bClientFireHeldHeartbeatRunning)
	{
		return;
	}

	bClientFireHeldHeartbeatRunning = true;

	World->GetTimerManager().SetTimer(
		ClientFireHeldHeartbeatHandle,
		this,
		&ThisClass::ClientFireHeldHeartbeatTick,
		FMath::Max(0.01f, ClientFireHeldHeartbeatIntervalSec),
		true);
}

void UMosesCombatComponent::StopClientFireHeldHeartbeat()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClientFireHeldHeartbeatHandle);
	}

	bClientFireHeldHeartbeatRunning = false;
}

void UMosesCombatComponent::ClientFireHeldHeartbeatTick()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		StopClientFireHeldHeartbeat();
		return;
	}

	APawn* Pawn = MosesCombat_Private::GetOwnerPawn(this);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	// Pawn/PC가 깨졌으면 “클라 heartbeat만” 끄고 서버에도 Stop 한번 날림
	if (!Pawn || !PC || !PC->IsLocalController())
	{
		StopClientFireHeldHeartbeat();
		ServerStopFire(); // ✅ RequestStopFire 재진입 제거
		return;
	}

	// ✅ 서버로 heartbeat 전송
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// ListenServer 로컬 입력도 서버값 갱신
		if (UWorld* World = GetWorld())
		{
			LastFireHeldHeartbeatSec = World->GetTimeSeconds();
		}
	}
	else
	{
		Server_UpdateFireHeldHeartbeat();
	}

	// ✅ Released 씹힘 대응: 실제 입력 상태로 Stop 강제
	const bool bMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
	if (!bMouseDown)
	{
		StopClientFireHeldHeartbeat();
		ServerStopFire(); // ✅ RequestStopFire 재진입 제거
	}
}

void UMosesCombatComponent::Server_ConsumeAmmo_ManualCost(int32 Cost)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Cost = FMath::Max(0, Cost);
	if (Cost <= 0)
	{
		return;
	}

	int32 Mag = 0;
	int32 ReserveCur = 0;
	int32 ReserveMax = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, ReserveCur, ReserveMax);

	const int32 OldMag = Mag;

	Mag = FMath::Max(0, Mag - Cost);
	SetSlotAmmo_Internal(CurrentSlot, Mag, ReserveCur, ReserveMax);

	BroadcastAmmoChanged(TEXT("Server_ConsumeAmmo_ManualCost"));

	UE_LOG(LogMosesWeapon, Verbose,
		TEXT("[AMMO][SV] ManualCost Slot=%d Cost=%d Mag %d->%d Reserve=%d/%d PS=%s"),
		CurrentSlot,
		Cost,
		OldMag,
		Mag,
		ReserveCur,
		ReserveMax,
		*GetNameSafe(GetOwner()));

	// (선택) 자동 리로드 정책을 Cost에도 동일 적용하고 싶으면 여기서 체크
	// 단, 이미 Server_ConsumeAmmo_OnApprovedFire에 auto reload가 있다면 중복 주의.
}

bool UMosesCombatComponent::Server_ApplyAmmoCostToSelf_GAS(float AmmoCost, const UMosesWeaponData* WeaponData) const
{
	// Server Authority only
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMosesGAS, Verbose, TEXT("[AMMO][SV][GAS] FAIL NoAuthority Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	AMosesPlayerState* SourcePS = Cast<AMosesPlayerState>(GetOwner());
	if (!SourcePS)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[AMMO][SV][GAS] FAIL OwnerNotPlayerState Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	UAbilitySystemComponent* SourceASC = SourcePS->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[AMMO][SV][GAS] FAIL NoSourceASC PS=%s"), *GetNameSafe(SourcePS));
		return false;
	}

	// Load GE (Soft Reference)
	TSubclassOf<UGameplayEffect> GEClass = AmmoCostGE_SetByCaller.LoadSynchronous();
	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Warning,
			TEXT("[AMMO][SV][GAS] FAIL AmmoCostGE missing -> Fallback. SoftPath=%s PS=%s"),
			*AmmoCostGE_SetByCaller.ToSoftObjectPath().ToString(),
			*GetNameSafe(SourcePS));
		return false;
	}

	// Build EffectContext
	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(const_cast<UMosesCombatComponent*>(this)); // SourceObject
	Ctx.AddSourceObject(const_cast<UMosesWeaponData*>(WeaponData)); // Optional (WeaponData can be null)

	// Make spec
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEClass, 1.0f, Ctx);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[AMMO][SV][GAS] FAIL MakeOutgoingSpec GE=%s PS=%s"),
			*GetNameSafe(GEClass.Get()), *GetNameSafe(SourcePS));
		return false;
	}

	// SetByCaller: Data.AmmoCost
	const float FinalCost = FMath::Abs(AmmoCost);
	SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_AmmoCost, FinalCost);

	// Apply to self (this will drive AttributeSet::PostGameplayEffectExecute on server)
	SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	UE_LOG(LogMosesWeapon, Warning,
		TEXT("[AMMO][SV][GAS] AmmoCost Applied Cost=%.2f GE=%s PS=%s"),
		FinalCost,
		*GetNameSafe(GEClass.Get()),
		*GetNameSafe(SourcePS));

	return true;
}