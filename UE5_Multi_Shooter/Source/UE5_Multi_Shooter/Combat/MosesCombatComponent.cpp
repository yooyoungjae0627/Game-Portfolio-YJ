#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"

#include "UE5_Multi_Shooter/Combat/MosesWeaponRegistrySubsystem.h"
#include "UE5_Multi_Shooter/Combat/MosesWeaponData.h"

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
}

bool UMosesCombatComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 1 && SlotIndex <= 3;
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

void UMosesCombatComponent::RequestEquipSlot(int32 SlotIndex)
{
	// 로컬 입력은 요청만 (서버가 확정)
	ServerEquipSlot(SlotIndex);
}

void UMosesCombatComponent::ServerEquipSlot_Implementation(int32 SlotIndex)
{
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

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] Equip Slot=%d Weapon=%s"),
		CurrentSlot, *WeaponId.ToString());

	// 서버에서 DataAsset resolve 로그
	LogResolveWeaponData_Server(WeaponId);

	// 서버 로컬(리슨 포함)에서도 즉시 코스메틱 갱신 이벤트 필요
	BroadcastEquippedChanged(TEXT("SV"));
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

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][SV] InitSlots S1=%s S2=%s S3=%s CurrentSlot=%d OwnerPS=%s"),
		*Slot1WeaponId.ToString(), *Slot2WeaponId.ToString(), *Slot3WeaponId.ToString(),
		CurrentSlot, *GetNameSafe(GetOwner()));

	// 리슨서버 즉시 표시용
	BroadcastEquippedChanged(TEXT("SV"));
}

void UMosesCombatComponent::Server_EnsureInitialized_Day2()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bInitialized_Day2)
	{
		return;
	}

	// 이미 값이 들어있으면 “초기화 완료”로 간주
	if (Slot1WeaponId.IsValid() && Slot2WeaponId.IsValid() && Slot3WeaponId.IsValid())
	{
		bInitialized_Day2 = true;
		return;
	}

	// [정책] 기본 슬롯은 태그로 고정 (프로젝트 GameplayTag 등록 필수)
	const FGameplayTag RifleA = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Rifle.A"));
	const FGameplayTag RifleB = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Rifle.B"));
	const FGameplayTag Pistol = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Pistol"));

	ServerInitDefaultSlots(RifleA, RifleB, Pistol);

	bInitialized_Day2 = true;

	UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][SV] EnsureInitialized_Day2 OK OwnerPS=%s"),
		*GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::OnRep_CurrentSlot()
{
	BroadcastEquippedChanged(TEXT("CL"));
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

void UMosesCombatComponent::BroadcastEquippedChanged(const TCHAR* ContextTag)
{
	const FGameplayTag EquippedId = GetEquippedWeaponId();
	if (!EquippedId.IsValid())
	{
		UE_LOG(LogMosesWeapon, Warning, TEXT("[WEAPON][%s] OnRep Equip FAIL Slot=%d WeaponId=INVALID OwnerPS=%s"),
			ContextTag, CurrentSlot, *GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogMosesWeapon, Log, TEXT("[WEAPON][%s] OnRep Equip Slot=%d Weapon=%s"),
		ContextTag, CurrentSlot, *EquippedId.ToString());

	OnEquippedChanged.Broadcast(CurrentSlot, EquippedId);
}

void UMosesCombatComponent::LogResolveWeaponData_Server(const FGameplayTag& WeaponId) const
{
	// 서버에서만 “증거 로그”를 찍는다.
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

	// RPM은 FireIntervalSec로부터 계산(0 방지)
	const float RPM = (Data->FireIntervalSec > 0.0001f) ? (60.0f / Data->FireIntervalSec) : 0.0f;

	UE_LOG(LogMosesWeapon, Log,
		TEXT("[WEAPON][SV] ResolveData Weapon=%s OK Damage=%.1f HS=%.2f RPM=%.0f Mag=%d Reserve=%d Reload=%.2f"),
		*WeaponId.ToString(),
		Data->Damage,
		Data->HeadshotMultiplier,
		RPM,
		Data->MagSize,
		Data->MaxReserve,
		Data->ReloadTime);
}
