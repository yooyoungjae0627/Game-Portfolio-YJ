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
#include "UE5_Multi_Shooter/GAS/MosesGameplayTags.h"

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
#include "DrawDebugHelpers.h"                
#include "Components/SkeletalMeshComponent.h" 

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
		UE_LOG(LogMosesCombat, Warning,
			TEXT("[WEAPON][SV] CombatComponent BeginPlay Owner=%s OwnerClass=%s DamageGE_SoftPath=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetOwner()->GetClass()),
			*DamageGE_SetByCaller.ToSoftObjectPath().ToString());
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

	DOREPLIFETIME(UMosesCombatComponent, SwapSerial);
	DOREPLIFETIME(UMosesCombatComponent, LastSwapFromSlot);
	DOREPLIFETIME(UMosesCombatComponent, LastSwapToSlot);

	DOREPLIFETIME(UMosesCombatComponent, Slot1ReserveMax);
	DOREPLIFETIME(UMosesCombatComponent, Slot2ReserveMax);
	DOREPLIFETIME(UMosesCombatComponent, Slot3ReserveMax);
	DOREPLIFETIME(UMosesCombatComponent, Slot4ReserveMax);
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
		OnRep_CurrentSlot(); // equipped slot UI sync
	}

	UE_LOG(LogMosesWeapon, Warning, TEXT("[AMMO][SV] DefaultRifleAmmo Granted Slot=1 Mag=30 Reserve=90/90 Weapon=%s"),
		*Slot1WeaponId.ToString());
}

// ============================================================================
// [AMMO_PICKUP] Ammo Farming - Server Authority ONLY
// - ReserveMax += ReserveMaxDelta
// - ReserveCur += ReserveFillDelta (Clamp 0..ReserveMax)
// - 적용 대상: "해당 AmmoType을 쓰는 무기"가 들어있는 슬롯들
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

		// ✅ AmmoType 일치 슬롯만 적용
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

	bIsReloading = true;
	OnRep_IsReloading(); // ✅ 여기서 RepNotify→Delegate가 나감(클라 HUD/코스메틱 트리거)

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

void UMosesCombatComponent::Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData)
{
	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	const int32 OldMag = Mag; // [MOD][AUTO-RELOAD]

	Mag = FMath::Max(Mag - 1, 0);
	SetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	BroadcastAmmoChanged(TEXT("Server_ConsumeAmmo_OnApprovedFire"));

	// [MOD][AUTO-RELOAD] 1->0 순간 + Reserve>0 이면 서버가 자동으로 Reload 시작
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

		// 수동(R)과 동일 루트로 진입
		ServerReload_Implementation();
	}
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// --------------------------------------------------------------------
	// 0) 카메라(시야) 기준 입력
	// --------------------------------------------------------------------
	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector AimDir = ViewRot.Vector();

	const float SpreadFactor = Server_CalcSpreadFactor01(WeaponData, OwnerPawn);

	float HalfAngleDeg = 0.0f;
	const FVector SpreadDir = Server_ApplySpreadToDirection(AimDir, WeaponData, SpreadFactor, HalfAngleDeg);

	// --------------------------------------------------------------------
	// 1) 총구(Muzzle) Start 확보 (히트스캔/프로젝트 공통)
	// --------------------------------------------------------------------
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

				// 손 무기 메시로 고정
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

	// --------------------------------------------------------------------
	// 2) Projectile weapon (Grenade Launcher)
	// --------------------------------------------------------------------
	if (WeaponData && WeaponData->bIsProjectileWeapon)
	{
		Server_SpawnGrenadeProjectile(WeaponData, MuzzleStart, SpreadDir, Controller, OwnerPawn);

		UE_LOG(LogMosesCombat, Warning,
			TEXT("[GRENADE][SV] FireRequest Weapon=%s Muzzle=%d SpawnLoc=%s ViewLoc=%s Dir=%s"),
			*WeaponData->WeaponId.ToString(),
			bGotMuzzle ? 1 : 0,
			*MuzzleStart.ToCompactString(),
			*ViewLoc.ToCompactString(),
			*SpreadDir.ToCompactString());

		return;
	}

	// --------------------------------------------------------------------
	// 3) [1차] 카메라 기준 Trace로 AimPoint 확보
	// --------------------------------------------------------------------
	const FVector CamStart = ViewLoc;
	const FVector CamEnd = CamStart + (SpreadDir * HitscanDistance);

	FCollisionQueryParams CamParams(SCENE_QUERY_STAT(Moses_FireTrace_Cam), true);
	CamParams.AddIgnoredActor(OwnerPawn);

	// (선택) Pawn Mesh도 무조건 무시하고 싶으면:
	if (USkeletalMeshComponent* MeshComp = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		CamParams.AddIgnoredComponent(MeshComp);
	}

	FHitResult CamHit;
	const bool bCamHit = World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, FireTraceChannel, CamParams);

	// ImpactPoint가 0으로 나오는 케이스가 있어서 Location 우선
	const FVector CamImpact = (bCamHit ? (CamHit.Location.IsNearlyZero() ? CamHit.ImpactPoint : CamHit.Location) : CamEnd);
	const FVector AimPoint = CamImpact;

	// --------------------------------------------------------------------
	// 4) [2차] 총구 기준 Trace로 최종 판정
	// --------------------------------------------------------------------
	FVector MuzzleEnd = AimPoint;

	// 너무 짧으면(거의 동일점) Trace가 의미없어져서 최소거리 보정
	const float DistMS = FVector::Dist(MuzzleStart, MuzzleEnd);
	if (DistMS < 10.0f)
	{
		MuzzleEnd = MuzzleStart + (SpreadDir * 10.0f);
	}

	FCollisionQueryParams MuzzleParams(SCENE_QUERY_STAT(Moses_FireTrace_Muzzle), true);
	MuzzleParams.AddIgnoredActor(OwnerPawn);

	// 손 무기 메시가 라인에 걸리는 경우 방지(가능한 범위에서)
	{
		TInlineComponentArray<USkeletalMeshComponent*> SkelComps;
		OwnerPawn->GetComponents<USkeletalMeshComponent>(SkelComps);
		for (USkeletalMeshComponent* Comp : SkelComps)
		{
			if (!Comp)
			{
				continue;
			}

			// Pawn 본체 메시는 물론, 손 무기 메시는 무시
			if (Comp == OwnerPawn->FindComponentByClass<USkeletalMeshComponent>() || Comp->GetFName() == TEXT("WeaponMesh_Hand"))
			{
				MuzzleParams.AddIgnoredComponent(Comp);
			}
		}
	}

	FHitResult FinalHit;
	const bool bFinalHit = World->LineTraceSingleByChannel(FinalHit, MuzzleStart, MuzzleEnd, FireTraceChannel, MuzzleParams);

	const FVector FinalImpact = (bFinalHit ? (FinalHit.Location.IsNearlyZero() ? FinalHit.ImpactPoint : FinalHit.Location) : MuzzleEnd);

	// --------------------------------------------------------------------
	// ✅ 4.5) Debug Draw (게임 화면에서 보이게 = 클라에서 DrawDebug)
	// - 서버가 계산한 값을 Multicast로 뿌려서 각 클라가 그리게 한다.
	// --------------------------------------------------------------------
	if (bServerTraceDebugDraw)
	{
		Multicast_DrawFireTraceDebug(
			CamStart, CamEnd,
			bCamHit, CamImpact,
			MuzzleStart, MuzzleEnd,
			bFinalHit, FinalImpact);
	}

	// --------------------------------------------------------------------
	// 5) Evidence-First 로그
	// --------------------------------------------------------------------
	UE_LOG(LogMosesCombat, Warning,
		TEXT("[TRACE][SV] Weapon=%s Spread=%.2f HalfAngle=%.2f Muzzle=%d CamHit=%d FinalHit=%d CamStart=%s CamEnd=%s MuzzleStart=%s AimPoint=%s FinalActor=%s Bone=%s"),
		WeaponData ? *WeaponData->WeaponId.ToString() : TEXT("None"),
		SpreadFactor,
		HalfAngleDeg,
		bGotMuzzle ? 1 : 0,
		bCamHit ? 1 : 0,
		bFinalHit ? 1 : 0,
		*CamStart.ToCompactString(),
		*CamEnd.ToCompactString(),
		*MuzzleStart.ToCompactString(),
		*AimPoint.ToCompactString(),
		*GetNameSafe(FinalHit.GetActor()),
		*FinalHit.BoneName.ToString());

	// --------------------------------------------------------------------
	// 6) 최종 피해 적용
	// --------------------------------------------------------------------
	if (!bFinalHit || !FinalHit.GetActor())
	{
		UE_LOG(LogMosesCombat, Verbose, TEXT("[HIT][SV] Miss (FinalTrace)"));
		return;
	}

	const bool bHeadshot = (FinalHit.BoneName == HeadshotBoneName);
	const float BaseDamage = WeaponData ? WeaponData->Damage : DefaultDamage;
	const float AppliedDamage = BaseDamage * (bHeadshot ? HeadshotDamageMultiplier : 1.0f);

	UE_LOG(LogMosesCombat, Warning, TEXT("[HIT][SV] Victim=%s Bone=%s Headshot=%d Damage=%.1f"),
		*GetNameSafe(FinalHit.GetActor()), *FinalHit.BoneName.ToString(), bHeadshot ? 1 : 0, AppliedDamage);

	const bool bAppliedByGAS = Server_ApplyDamageToTarget_GAS(FinalHit.GetActor(), AppliedDamage, Controller, OwnerPawn, WeaponData);
	if (!bAppliedByGAS)
	{
		UGameplayStatics::ApplyDamage(FinalHit.GetActor(), AppliedDamage, Controller, OwnerPawn, nullptr);

		UE_LOG(LogMosesCombat, Warning, TEXT("[HIT][SV] Fallback ApplyDamage Victim=%s Damage=%.1f"),
			*GetNameSafe(FinalHit.GetActor()), AppliedDamage);
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

	// ProjectileClass는 WeaponData 단일 진실
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

	// --------------------------------------------------------------------
	// [MOD] Damage GE (없어도 스폰은 진행)
	// - GE가 없으면 Projectile 내부에서 GAS 적용은 실패 → ApplyDamage 폴백으로 감
	// --------------------------------------------------------------------
	TSubclassOf<UGameplayEffect> GEClass = DamageGE_SetByCaller.LoadSynchronous();
	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Warning,
			TEXT("[GRENADE][SV] DamageGE missing -> FallbackOnly. SoftPath=%s"),
			*DamageGE_SetByCaller.ToSoftObjectPath().ToString());
		// return;  // ❌ 막지 않는다
	}

	FActorSpawnParameters Params;
	Params.Owner = OwnerPawn; // Pawn으로 고정
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
		GEClass, // null일 수도 있음(폴백용)
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


// ============================================================================
// UMosesCombatComponent::Server_ApplyDamageToTarget_GAS (FULL)  [MOD]
// ----------------------------------------------------------------------------
// 목표:
// - 서버 권위 100%
// - "총 히트 대상"이 Pawn(플레이어 캐릭터)인 경우, ASC가 PlayerState(SSOT)에 있으므로
//   TargetASC를 Pawn->PlayerState->ASC로 우회해서 데미지 GE를 적용한다.
// - 좀비/기타 대상이 IAbilitySystemInterface를 구현한 경우(ASC가 Pawn에 있는 경우)는
//   기존대로 TargetActor의 ASI를 사용한다.
// - 실패 시 false 반환하여 호출부에서 ApplyDamage 폴백(디버그) 가능
// ----------------------------------------------------------------------------
// 기대 로그:
// - 성공: [GAS][SV] APPLY OK TargetASC=... TargetActor=... Damage=...
// - 실패: [GAS][SV] APPLY FAIL (...)
// ============================================================================

bool UMosesCombatComponent::Server_ApplyDamageToTarget_GAS(
	AActor* TargetActor,
	float Damage,
	AController* InstigatorController,
	AActor* DamageCauser,
	const UMosesWeaponData* WeaponData) const
{

	// [MOD] Dead target guard (PS SSOT)
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
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NotAuthority)"));
		return false;
	}

	if (!TargetActor)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (Target=null)"));
		return false;
	}

	AMosesPlayerState* SourcePS = Cast<AMosesPlayerState>(GetOwner());
	if (!SourcePS)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NoOwnerPS) Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	UAbilitySystemComponent* SourceASC = SourcePS->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NoSourceASC) PS=%s"), *GetNameSafe(SourcePS));
		return false;
	}

	// ---- Target ASC resolve (ASI or Pawn->PlayerState) ----
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
			UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (PawnHasNoPlayerState) Pawn=%s"), *GetNameSafe(TargetPawn));
			return false;
		}
	}
	else
	{
		UE_LOG(LogMosesGAS, Warning,
			TEXT("[GAS][SV] APPLY FAIL (TargetNoASI_AndNotPawn) Target=%s Class=%s"),
			*GetNameSafe(TargetActor),
			*GetNameSafe(TargetActor ? TargetActor->GetClass() : nullptr));
		return false;
	}

	if (!TargetASC)
	{
		UE_LOG(LogMosesGAS, Warning, TEXT("[GAS][SV] APPLY FAIL (NoTargetASC) Target=%s"), *GetNameSafe(ResolvedTargetOwnerForLog));
		return false;
	}

	// ---- Damage GE resolve ----
	TSubclassOf<UGameplayEffect> GEClass = DamageGE_SetByCaller.LoadSynchronous();
	if (!GEClass)
	{
		UE_LOG(LogMosesGAS, Error,
			TEXT("[GAS][SV] APPLY FAIL (NoDamageGE) SoftPath=%s Owner=%s OwnerClass=%s"),
			*DamageGE_SetByCaller.ToSoftObjectPath().ToString(),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetOwner() ? GetOwner()->GetClass() : nullptr));
		return false;
	}

	// ---- Context / Spec ----
	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddInstigator(DamageCauser, InstigatorController);
	Ctx.AddSourceObject(this);
	Ctx.AddSourceObject(WeaponData);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEClass, 1.0f, Ctx);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogMosesGAS, Warning,
			TEXT("[GAS][SV] APPLY FAIL (SpecInvalid) Target=%s GE=%s"),
			*GetNameSafe(ResolvedTargetOwnerForLog),
			*GetNameSafe(GEClass.Get()));
		return false;
	}

	const float FinalDamageForSetByCaller = FMath::Abs(Damage);
	SpecHandle.Data->SetSetByCallerMagnitude(FMosesGameplayTags::Get().Data_Damage, FinalDamageForSetByCaller);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogMosesGAS, Warning,
		TEXT("[GAS][SV] APPLY OK TargetActor=%s ResolvedOwner=%s TargetASC_Owner=%s Damage=%.1f Weapon=%s"),
		*GetNameSafe(TargetActor),
		*GetNameSafe(ResolvedTargetOwnerForLog),
		*GetNameSafe(TargetASC ? TargetASC->GetOwner() : nullptr),
		FinalDamageForSetByCaller,
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

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[FIRE][SV] PropagateCosmetic OK ShooterPawn=%s Weapon=%s"),
		*GetNameSafe(PlayerChar),
		*ApprovedWeaponId.ToString());

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

	// Reload/Swap 같은 상태도 서버에서 확실히 끊어주는 게 안정적
	bIsReloading = false;

	// 서버 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	// RepNotify/Delegate
	OnRep_IsDead();
	OnRep_IsReloading();

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[DEAD][SV][CC] MarkDead bIsDead=1 PS=%s PS_Pawn=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(MosesCombat_Private::GetOwnerPawn(this)));

	// ❌ 몽타주는 여기서 절대 재생하지 않는다.
}


// ============================================================================
// [FIX] UMosesCombatComponent::ServerClearDeadAfterRespawn
// - MatchGameMode / PlayerState에서 호출 중인데 정의가 없어서 LNK2019 발생
// - 서버 권위에서만 Dead 상태/Reload 상태/타이머를 정리하고 RepNotify->Delegate 갱신
// ============================================================================

void UMosesCombatComponent::ServerClearDeadAfterRespawn()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bIsDead && !bIsReloading)
	{
		return;
	}

	const bool bOldDead = bIsDead;
	const bool bOldReload = bIsReloading;

	bIsDead = false;
	bIsReloading = false;

	// 서버 타이머 정리(리스폰 직후 잔존 타이머 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	// RepNotify/Delegate (ListenServer 즉시 반영 패턴 유지)
	if (bOldDead)
	{
		OnRep_IsDead();
	}
	if (bOldReload)
	{
		OnRep_IsReloading();
	}

	UE_LOG(LogMosesCombat, Warning,
		TEXT("[RESPAWN][SV][CC] ClearDead DONE Dead %d->0 Reload %d->0 PS=%s Pawn=%s"),
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

// ============================================================================
// Broadcast (Ammo) - RepNotify -> Delegate
// - 기존 2파라미터 유지 + 신규 3파라미터(ReserveMax) 발행
// ============================================================================
void UMosesCombatComponent::BroadcastAmmoChanged(const TCHAR* ContextTag)
{
	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Cur, Max);

	// 기존 호환
	OnAmmoChanged.Broadcast(Mag, Cur);

	// 신규(ReserveMax 포함)
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

// ============================================================================
// [OPTIONAL BUT RECOMMENDED] 초기 슬롯 Ammo가 0/0/0일 때 WeaponData로 초기화
// - WeaponData: MagSize, MaxReserve 사용
// - 기본 정책: Mag=MagSize, ReserveCur=MaxReserve, ReserveMax=MaxReserve
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

	int32 Mag = 0, Cur = 0, Max = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Cur, Max);

	// 이미 초기화되어 있으면 보호
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
// Ammo helpers (ReserveMax 포함) - 내부 저장/조회
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
		OnRep_Slot1Ammo(); // ListenServer 즉시 반영
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
	// ReserveMax는 "현재 값 유지"가 기본 정책
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
	// 0 = 전체 갱신, 1~4 = 해당 슬롯 중심 갱신
	OnSlotsStateChanged.Broadcast(ChangedSlotOr0ForAll);

	UE_LOG(LogMosesWeapon, Verbose, TEXT("[WEAPON][HUD] SlotsStateChanged Slot=%d Ctx=%s PS=%s"),
		ChangedSlotOr0ForAll,
		ContextTag ? ContextTag : TEXT("None"),
		*GetNameSafe(GetOwner()));
}

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

	// ✅ Tag -> Enum 매핑 (현재 프로젝트는 enum 기반 AddAmmoByType을 이미 구현했으니 재사용)
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
// [MOD][DEBUG] Trace Debug (Server -> All Clients)
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
	// Dedicated Server는 렌더링 없음
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 게임 플레이 월드에서만(에디터 프리뷰/에디터월드 꼬임 방지)
	if (!World->IsGameWorld())
	{
		return;
	}

	const float Life = FMath::Max(0.01f, ServerTraceDebugDrawTime);

	// 시야(Cam) Trace
	DrawDebugLine(World, CamStart, CamEnd, FColor::Cyan, false, Life, 0, 1.5f);
	if (bCamHit)
	{
		DrawDebugPoint(World, CamImpact, 10.f, FColor::Cyan, false, Life);
	}

	// 총구(Muzzle) 최종 판정 Trace
	DrawDebugLine(World, MuzzleStart, MuzzleEnd, bFinalHit ? FColor::Green : FColor::Red, false, Life, 0, 2.5f);
	if (bFinalHit)
	{
		DrawDebugPoint(World, FinalImpact, 14.f, FColor::Green, false, Life);
	}
#endif // ENABLE_DRAW_DEBUG
}

