#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "UE5_Multi_Shooter/Combat/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"

#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "UE5_Multi_Shooter/Character/PlayerCharacter.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/GAS/Components/MosesAbilitySystemComponent.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UMosesCombatComponent::UMosesCombatComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
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

	// [ADD]
	DOREPLIFETIME(UMosesCombatComponent, bIsDead);
}

// ============================================================================
// Queries
// ============================================================================

bool UMosesCombatComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return (SlotIndex >= 1 && SlotIndex <= 3);
}

FGameplayTag UMosesCombatComponent::GetSlotWeaponIdInternal(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 1: return Slot1WeaponId;
	case 2: return Slot2WeaponId;
	case 3: return Slot3WeaponId;
	default: break;
	}
	return FGameplayTag();
}

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
// Equip (Client -> Server)
// ============================================================================

void UMosesCombatComponent::RequestEquipSlot(int32 SlotIndex)
{
	ServerEquipSlot(SlotIndex);
}

void UMosesCombatComponent::ServerEquipSlot_Implementation(int32 SlotIndex)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted ServerEquipSlot"));

	// [ADD] 죽었으면 장착도 금지(최소 정책)
	if (bIsDead)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[GUARD][SV] Reject Equip(IsDead) Slot=%d OwnerPS=%s"),
			SlotIndex, *GetNameSafe(GetOwner()));
		return;
	}

	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip FAIL InvalidSlot=%d OwnerPS=%s"),
			SlotIndex, *GetNameSafe(GetOwner()));
		return;
	}

	const FGameplayTag WeaponId = GetSlotWeaponIdInternal(SlotIndex);
	if (!WeaponId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] Equip FAIL Slot=%d WeaponId=INVALID OwnerPS=%s"),
			SlotIndex, *GetNameSafe(GetOwner()));
		return;
	}

	CurrentSlot = SlotIndex;

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] Equip Slot=%d Weapon=%s OwnerPS=%s"),
		CurrentSlot, *WeaponId.ToString(), *GetNameSafe(GetOwner()));

	LogResolveWeaponData_Server(WeaponId);
	Server_EnsureAmmoInitializedForSlot(SlotIndex, WeaponId);

	// Listen server 즉시 반영
	BroadcastEquippedChanged(TEXT("SV"));
	BroadcastAmmoChanged(TEXT("SV"));
}

void UMosesCombatComponent::ServerInitDefaultSlots(const FGameplayTag& InSlot1, const FGameplayTag& InSlot2, const FGameplayTag& InSlot3)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted ServerInitDefaultSlots"));

	Slot1WeaponId = InSlot1;
	Slot2WeaponId = InSlot2;
	Slot3WeaponId = InSlot3;
	CurrentSlot = 1;

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] InitSlots S1=%s S2=%s S3=%s CurrentSlot=%d OwnerPS=%s"),
		*Slot1WeaponId.ToString(), *Slot2WeaponId.ToString(), *Slot3WeaponId.ToString(),
		CurrentSlot, *GetNameSafe(GetOwner()));

	Server_EnsureAmmoInitializedForSlot(1, Slot1WeaponId);
	Server_EnsureAmmoInitializedForSlot(2, Slot2WeaponId);
	Server_EnsureAmmoInitializedForSlot(3, Slot3WeaponId);

	BroadcastEquippedChanged(TEXT("SV"));
	BroadcastAmmoChanged(TEXT("SV"));
}

void UMosesCombatComponent::Server_EnsureInitialized_Day2()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted Server_EnsureInitialized_Day2"));

	if (bInitialized_Day2)
	{
		return;
	}

	if (Slot1WeaponId.IsValid() && Slot2WeaponId.IsValid() && Slot3WeaponId.IsValid())
	{
		Server_EnsureAmmoInitializedForSlot(1, Slot1WeaponId);
		Server_EnsureAmmoInitializedForSlot(2, Slot2WeaponId);
		Server_EnsureAmmoInitializedForSlot(3, Slot3WeaponId);

		bInitialized_Day2 = true;
		return;
	}

	const FGameplayTag RifleA = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Rifle.A"));
	const FGameplayTag RifleB = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Rifle.B"));
	const FGameplayTag Pistol = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Pistol"));

	ServerInitDefaultSlots(RifleA, RifleB, Pistol);

	bInitialized_Day2 = true;

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] EnsureInitialized_Day2 OK OwnerPS=%s"),
		*GetNameSafe(GetOwner()));
}

// ============================================================================
// RepNotifies -> Delegates
// ============================================================================

void UMosesCombatComponent::OnRep_CurrentSlot()
{
	BroadcastEquippedChanged(TEXT("CL"));
	BroadcastAmmoChanged(TEXT("CL"));
}

void UMosesCombatComponent::OnRep_Slot1WeaponId()
{
	BroadcastEquippedChanged(TEXT("CL"));
}

void UMosesCombatComponent::OnRep_Slot2WeaponId()
{
	BroadcastEquippedChanged(TEXT("CL"));
}

void UMosesCombatComponent::OnRep_Slot3WeaponId()
{
	BroadcastEquippedChanged(TEXT("CL"));
}

void UMosesCombatComponent::OnRep_Slot1Ammo()
{
	BroadcastAmmoChanged(TEXT("CL"));
}

void UMosesCombatComponent::OnRep_Slot2Ammo()
{
	BroadcastAmmoChanged(TEXT("CL"));
}

void UMosesCombatComponent::OnRep_Slot3Ammo()
{
	BroadcastAmmoChanged(TEXT("CL"));
}

// [ADD]
void UMosesCombatComponent::OnRep_IsDead()
{
	BroadcastDeadChanged(TEXT("CL"));
}

void UMosesCombatComponent::BroadcastEquippedChanged(const TCHAR* ContextTag)
{
	const FGameplayTag EquippedId = GetEquippedWeaponId();
	if (!EquippedId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][%s] Equipped INVALID Slot=%d OwnerPS=%s"),
			ContextTag, CurrentSlot, *GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][%s] Equipped Slot=%d Weapon=%s OwnerPS=%s"),
		ContextTag, CurrentSlot, *EquippedId.ToString(), *GetNameSafe(GetOwner()));

	OnEquippedChanged.Broadcast(CurrentSlot, EquippedId);
}

void UMosesCombatComponent::BroadcastAmmoChanged(const TCHAR* ContextTag)
{
	const int32 Mag = GetCurrentMagAmmo();
	const int32 Reserve = GetCurrentReserveAmmo();

	UE_LOG(LogMosesCombat, Verbose, TEXT("[AMMO][%s] Slot=%d %d/%d OwnerPS=%s"),
		ContextTag, CurrentSlot, Mag, Reserve, *GetNameSafe(GetOwner()));

	OnAmmoChanged.Broadcast(Mag, Reserve);
}

// [ADD]
void UMosesCombatComponent::BroadcastDeadChanged(const TCHAR* ContextTag)
{
	UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][%s] Dead=%d OwnerPS=%s"),
		ContextTag, bIsDead ? 1 : 0, *GetNameSafe(GetOwner()));

	OnDeadChanged.Broadcast(bIsDead);
}

// ============================================================================
// Ammo helpers
// ============================================================================

void UMosesCombatComponent::GetSlotAmmo_Internal(int32 SlotIndex, int32& OutMag, int32& OutReserve) const
{
	OutMag = 0;
	OutReserve = 0;

	switch (SlotIndex)
	{
	case 1: OutMag = Slot1MagAmmo; OutReserve = Slot1ReserveAmmo; break;
	case 2: OutMag = Slot2MagAmmo; OutReserve = Slot2ReserveAmmo; break;
	case 3: OutMag = Slot3MagAmmo; OutReserve = Slot3ReserveAmmo; break;
	default: break;
	}
}

void UMosesCombatComponent::SetSlotAmmo_Internal(int32 SlotIndex, int32 NewMag, int32 NewReserve)
{
	switch (SlotIndex)
	{
	case 1: Slot1MagAmmo = NewMag; Slot1ReserveAmmo = NewReserve; break;
	case 2: Slot2MagAmmo = NewMag; Slot2ReserveAmmo = NewReserve; break;
	case 3: Slot3MagAmmo = NewMag; Slot3ReserveAmmo = NewReserve; break;
	default: break;
	}
}

void UMosesCombatComponent::Server_EnsureAmmoInitializedForSlot(int32 SlotIndex, const FGameplayTag& WeaponId)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted Server_EnsureAmmoInitializedForSlot"));

	if (!IsValidSlotIndex(SlotIndex) || !WeaponId.IsValid())
	{
		return;
	}

	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(SlotIndex, Mag, Reserve);

	// 최초 1회만 채움
	if (Mag > 0 || Reserve > 0)
	{
		return;
	}

	const AActor* OwnerActor = Cast<AActor>(GetOwner());
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return;
	}

	const UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data)
	{
		return;
	}

	SetSlotAmmo_Internal(SlotIndex, Data->MagSize, Data->MaxReserve);

	UE_LOG(LogMosesCombat, Warning, TEXT("[AMMO][SV] Init Slot=%d Weapon=%s %d/%d OwnerPS=%s"),
		SlotIndex, *WeaponId.ToString(), Data->MagSize, Data->MaxReserve, *GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::LogResolveWeaponData_Server(const FGameplayTag& WeaponId) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const AActor* OwnerActor = Cast<AActor>(GetOwner());
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] ResolveData FAIL NoGameInstance Weapon=%s"),
			*WeaponId.ToString());
		return;
	}

	const UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] ResolveData FAIL NoRegistry Weapon=%s"),
			*WeaponId.ToString());
		return;
	}

	const UMosesWeaponData* Data = Registry->ResolveWeaponData(WeaponId);
	if (!Data)
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] ResolveData FAIL Weapon=%s"),
			*WeaponId.ToString());
		return;
	}

	const float RPM = (Data->FireIntervalSec > 0.0001f) ? (60.0f / Data->FireIntervalSec) : 0.0f;

	UE_LOG(LogMosesWeapon, Log,
		TEXT("[WEAPON][SV] ResolveData OK Weapon=%s Damage=%.1f HS=%.2f RPM=%.0f Mag=%d Reserve=%d Reload=%.2f"),
		*WeaponId.ToString(),
		Data->Damage,
		Data->HeadshotMultiplier,
		RPM,
		Data->MagSize,
		Data->MaxReserve,
		Data->ReloadTime);
}

// ============================================================================
// Fire
// ============================================================================

void UMosesCombatComponent::RequestFire()
{
	ServerFire();
}

void UMosesCombatComponent::ServerFire_Implementation()
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted ServerFire"));

	EMosesFireGuardFailReason Reason = EMosesFireGuardFailReason::None;
	FString Debug;

	if (!Server_CanFire(Reason, Debug))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=%d Debug=%s OwnerPS=%s"),
			static_cast<int32>(Reason), *Debug, *GetNameSafe(GetOwner()));
		return;
	}

	FGameplayTag WeaponId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(WeaponId);
	if (!WeaponData)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=NoWeaponData OwnerPS=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	AMosesPlayerState* OwnerPS = Cast<AMosesPlayerState>(GetOwner());
	APawn* ShooterPawn = OwnerPS ? OwnerPS->GetPawn() : nullptr;
	AController* ShooterController = ShooterPawn ? ShooterPawn->GetController() : nullptr;

	if (!ShooterPawn || !ShooterController)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=NoPawnOrController OwnerPS=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	const int32 AmmoBefore = GetCurrentMagAmmo();

	UE_LOG(LogMosesCombat, Log, TEXT("[FIRE][SV] Req Player=%s Weapon=%s AmmoBefore=%d"),
		*GetNameSafe(ShooterPawn), *WeaponId.ToString(), AmmoBefore);

	// 승인: 탄약/쿨타임
	if (!Server_IsFireCooldownReady(WeaponData))
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=Cooldown Player=%s Weapon=%s"),
			*GetNameSafe(ShooterPawn), *WeaponId.ToString());
		return;
	}

	if (GetCurrentMagAmmo() <= 0)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[GUARD][SV] Reject Fire Reason=NoAmmo Player=%s Weapon=%s"),
			*GetNameSafe(ShooterPawn), *WeaponId.ToString());
		return;
	}

	Server_ConsumeAmmo_OnApprovedFire(WeaponData);
	Server_UpdateFireCooldownStamp();

	const int32 AmmoAfter = GetCurrentMagAmmo();

	UE_LOG(LogMosesCombat, Log, TEXT("[FIRE][SV] Approved AmmoAfter=%d Cooldown=%.3f"),
		AmmoAfter, WeaponData->FireIntervalSec);

	// Listen 서버 즉시 HUD
	BroadcastAmmoChanged(TEXT("SV"));

	// 서버 승인 후 코스메틱
	if (APlayerCharacter* ShooterPC = Cast<APlayerCharacter>(ShooterPawn))
	{
		ShooterPC->Multicast_PlayFireMontage();
	}

	// Trace
	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	ShooterController->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector TraceStart = ViewLoc;
	const FVector TraceEnd = TraceStart + (ViewRot.Vector() * HitscanDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MosesFireTrace), true);
	Params.AddIgnoredActor(ShooterPawn);

	FHitResult Hit;
	UWorld* World = ShooterPawn->GetWorld();
	if (!World)
	{
		return;
	}

	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, FireTraceChannel, Params);
	if (!bHit || !Hit.GetActor())
	{
		return;
	}

	AActor* VictimActor = Hit.GetActor();
	const FName Bone = Hit.BoneName;

	const bool bHeadshot = (Bone == HeadshotBoneName);

	float FinalDamage = WeaponData->Damage;
	if (bHeadshot)
	{
		FinalDamage *= WeaponData->HeadshotMultiplier;
	}

	UE_LOG(LogMosesCombat, Log, TEXT("[HIT][SV] Victim=%s Bone=%s Dmg=%.1f Head=%d"),
		*GetNameSafe(VictimActor), *Bone.ToString(), FinalDamage, bHeadshot ? 1 : 0);

	// Victim resolve
	AMosesCharacter* VictimPawn = Cast<AMosesCharacter>(VictimActor);
	AMosesPlayerState* VictimPS = VictimPawn ? VictimPawn->GetPlayerState<AMosesPlayerState>() : nullptr;

	if (!VictimPawn || !VictimPS)
	{
		return;
	}

	// Victim CombatComponent(Death SSOT를 여기서 확정)
	UMosesCombatComponent* VictimCombat = VictimPS->FindComponentByClass<UMosesCombatComponent>();
	if (VictimCombat && VictimCombat->IsDead())
	{
		// [ADD] 이미 죽은 상태면 중복 처리 금지(증거 로그만)
		UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][SV] Skip Damage (AlreadyDead) VictimPS=%s"), *GetNameSafe(VictimPS));
		return;
	}

	UMosesAbilitySystemComponent* VictimASC = Cast<UMosesAbilitySystemComponent>(VictimPS->GetAbilitySystemComponent());
	if (!VictimASC)
	{
		return;
	}

	const float CurHP = VictimASC->GetNumericAttribute(UMosesAttributeSet::GetHealthAttribute());
	const float MaxHP = VictimASC->GetNumericAttribute(UMosesAttributeSet::GetMaxHealthAttribute());
	const float NewHP = FMath::Clamp(CurHP - FinalDamage, 0.0f, MaxHP);

	VictimASC->SetNumericAttributeBase(UMosesAttributeSet::GetHealthAttribute(), NewHP);

	UE_LOG(LogMosesCombat, Log, TEXT("[HP][SV] Victim=%s HP %.1f -> %.1f"), *GetNameSafe(VictimPawn), CurHP, NewHP);

	// 서버 훅(프로젝트 구현)
	VictimPawn->ServerNotifyDamaged(FinalDamage, ShooterPawn);

	if (APlayerCharacter* VictimPC = Cast<APlayerCharacter>(VictimPawn))
	{
		VictimPC->Multicast_PlayHitReactMontage();
	}
	else
	{
		VictimPawn->Multicast_PlayHitReactCosmetic();
	}

	// Death 확정
	if (NewHP <= 0.0f)
	{
		// [ADD] Victim의 SSOT(CombatComponent)에서 Dead를 서버 확정 + 복제
		Victim_ServerSetDead_IfNeeded(VictimPS, ShooterPawn);

		// 프로젝트 스탯/후처리
		VictimPS->ServerAddDeath();
		VictimPawn->ServerNotifyDeath(ShooterPawn);

		if (APlayerCharacter* VictimPC2 = Cast<APlayerCharacter>(VictimPawn))
		{
			VictimPC2->Multicast_PlayDeathMontage();
		}
		else
		{
			VictimPawn->Multicast_PlayDeathCosmetic();
		}
	}
}

bool UMosesCombatComponent::Server_CanFire(EMosesFireGuardFailReason& OutReason, FString& OutDebug) const
{
	OutReason = EMosesFireGuardFailReason::None;
	OutDebug.Reset();

	// [ADD] 죽은 상태면 발사 요청 자체를 거절
	if (bIsDead)
	{
		OutReason = EMosesFireGuardFailReason::IsDead;
		OutDebug = TEXT("ShooterIsDead");
		return false;
	}

	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (!PS)
	{
		OutReason = EMosesFireGuardFailReason::NoOwner;
		OutDebug = TEXT("NoOwnerPS");
		return false;
	}

	const APawn* Pawn = PS->GetPawn();
	if (!Pawn)
	{
		OutReason = EMosesFireGuardFailReason::NoPawn;
		OutDebug = TEXT("NoPawn");
		return false;
	}

	const AController* Controller = Pawn->GetController();
	if (!Controller)
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

	FGameplayTag DummyId;
	const UMosesWeaponData* WeaponData = Server_ResolveEquippedWeaponData(DummyId);
	if (!WeaponData)
	{
		OutReason = EMosesFireGuardFailReason::NoWeaponData;
		OutDebug = TEXT("NoWeaponData");
		return false;
	}

	if (!Server_IsFireCooldownReady(WeaponData))
	{
		OutReason = EMosesFireGuardFailReason::Cooldown;
		OutDebug = TEXT("CooldownNotReady");
		return false;
	}

	if (GetCurrentMagAmmo() <= 0)
	{
		OutReason = EMosesFireGuardFailReason::NoAmmo;
		OutDebug = TEXT("NoMagAmmo");
		return false;
	}

	return true;
}

const UMosesWeaponData* UMosesCombatComponent::Server_ResolveEquippedWeaponData(FGameplayTag& OutWeaponId) const
{
	OutWeaponId = GetEquippedWeaponId();
	if (!OutWeaponId.IsValid())
	{
		return nullptr;
	}

	const AActor* OwnerActor = Cast<AActor>(GetOwner());
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	const UMosesWeaponRegistrySubsystem* Registry = GI->GetSubsystem<UMosesWeaponRegistrySubsystem>();
	if (!Registry)
	{
		return nullptr;
	}

	return Registry->ResolveWeaponData(OutWeaponId);
}

bool UMosesCombatComponent::Server_IsFireCooldownReady(const UMosesWeaponData* WeaponData) const
{
	if (!WeaponData)
	{
		return false;
	}

	const AActor* OwnerActor = Cast<AActor>(GetOwner());
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();

	double Last = -9999.0;
	switch (CurrentSlot)
	{
	case 1: Last = Slot1LastFireTimeSec; break;
	case 2: Last = Slot2LastFireTimeSec; break;
	case 3: Last = Slot3LastFireTimeSec; break;
	default: break;
	}

	return (Now - Last) >= static_cast<double>(WeaponData->FireIntervalSec);
}

void UMosesCombatComponent::Server_UpdateFireCooldownStamp()
{
	const AActor* OwnerActor = Cast<AActor>(GetOwner());
	const UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	switch (CurrentSlot)
	{
	case 1: Slot1LastFireTimeSec = Now; break;
	case 2: Slot2LastFireTimeSec = Now; break;
	case 3: Slot3LastFireTimeSec = Now; break;
	default: break;
	}
}

void UMosesCombatComponent::Server_ConsumeAmmo_OnApprovedFire(const UMosesWeaponData* WeaponData)
{
	MOSES_GUARD_AUTHORITY_VOID(this, "Combat", TEXT("Client attempted Server_ConsumeAmmo_OnApprovedFire"));

	int32 Mag = 0;
	int32 Reserve = 0;
	GetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	Mag = FMath::Max(0, Mag - 1);
	SetSlotAmmo_Internal(CurrentSlot, Mag, Reserve);

	// Listen 서버 즉시 HUD
	BroadcastAmmoChanged(TEXT("SV"));
}

// [ADD] Victim Death SSOT 확정(중복 방지 포함)
void UMosesCombatComponent::Victim_ServerSetDead_IfNeeded(AMosesPlayerState* VictimPS, APawn* KillerPawn)
{
	if (!VictimPS)
	{
		return;
	}

	UMosesCombatComponent* VictimCombat = VictimPS->FindComponentByClass<UMosesCombatComponent>();
	if (!VictimCombat)
	{
		UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][SV] Victim has no CombatComponent VictimPS=%s"), *GetNameSafe(VictimPS));
		return;
	}

	if (VictimCombat->bIsDead)
	{
		return;
	}

	// 서버에서만 변경
	if (!VictimPS->HasAuthority())
	{
		return;
	}

	VictimCombat->bIsDead = true;

	UE_LOG(LogMosesCombat, Warning, TEXT("[DEAD][SV] SetDead OK VictimPS=%s Killer=%s"),
		*GetNameSafe(VictimPS), *GetNameSafe(KillerPawn));

	// Listen 서버(서버 로컬)는 OnRep가 없으므로 즉시 브로드캐스트
	VictimCombat->BroadcastDeadChanged(TEXT("SV"));
}
