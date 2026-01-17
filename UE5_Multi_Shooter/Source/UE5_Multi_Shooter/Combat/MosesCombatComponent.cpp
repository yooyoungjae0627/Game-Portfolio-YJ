#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesCombatComponent::UMosesCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Replicated data is arrays; ensure size is ready even before replication arrives.
	EnsureReplicatedArrays();

	// ✅ Day2 최소 기본값 (서버/클라 공통 기본값, 원하면 DataAsset/BP로 이동 가능)
	InitializeDefaultsIfNeeded();
}

void UMosesCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesCombatComponent, AmmoStates);
	DOREPLIFETIME(UMosesCombatComponent, WeaponSlots);
}

bool UMosesCombatComponent::HasAmmoState(EMosesWeaponType WeaponType) const
{
	// Array 기반에서는 "유효한 enum이면 상태 존재"로 취급.
	return IsValidWeaponType(WeaponType);
}

FAmmoState UMosesCombatComponent::GetAmmoState(EMosesWeaponType WeaponType) const
{
	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (AmmoStates.IsValidIndex(Index))
	{
		return AmmoStates[Index];
	}

	return FAmmoState();
}

void UMosesCombatComponent::Server_SetAmmoState(EMosesWeaponType WeaponType, const FAmmoState& NewState)
{
	check(GetOwner() && GetOwner()->HasAuthority());

	EnsureReplicatedArrays();
	InitializeDefaultsIfNeeded();

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!AmmoStates.IsValidIndex(Index))
	{
		return;
	}

	FAmmoState Clamped = NewState;
	Clamped.MagAmmo = ClampNonNegative(Clamped.MagAmmo);
	Clamped.ReserveAmmo = ClampNonNegative(Clamped.ReserveAmmo);
	Clamped.MaxMag = ClampNonNegative(Clamped.MaxMag);
	Clamped.MaxReserve = ClampNonNegative(Clamped.MaxReserve);

	Clamped.MagAmmo = FMath::Clamp(Clamped.MagAmmo, 0, Clamped.MaxMag);
	Clamped.ReserveAmmo = FMath::Clamp(Clamped.ReserveAmmo, 0, Clamped.MaxReserve);

	AmmoStates[Index] = Clamped;

	// 서버도 즉시 브로드캐스트(리스닝 서버 HUD/로그)
	OnAmmoChanged.Broadcast(WeaponType, Clamped);
}

void UMosesCombatComponent::Server_AddReserveAmmo(EMosesWeaponType WeaponType, int32 AddAmount)
{
	check(GetOwner() && GetOwner()->HasAuthority());

	EnsureReplicatedArrays();
	InitializeDefaultsIfNeeded();

	AddAmount = ClampNonNegative(AddAmount);

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!AmmoStates.IsValidIndex(Index))
	{
		return;
	}

	FAmmoState& State = AmmoStates[Index];
	if (State.MaxReserve <= 0 && State.MaxMag <= 0)
	{
		return;
	}

	State.ReserveAmmo = FMath::Clamp(State.ReserveAmmo + AddAmount, 0, State.MaxReserve);

	UE_LOG(LogMosesPickup, Log, TEXT("[AMMO] AddReserve Weapon=%s Add=%d Reserve=%d/%d"),
		*UEnum::GetValueAsString(WeaponType), AddAmount, State.ReserveAmmo, State.MaxReserve);

	OnAmmoChanged.Broadcast(WeaponType, State);
}

bool UMosesCombatComponent::Server_ConsumeMagAmmo(EMosesWeaponType WeaponType, int32 ConsumeAmount)
{
	check(GetOwner() && GetOwner()->HasAuthority());

	EnsureReplicatedArrays();
	InitializeDefaultsIfNeeded();

	ConsumeAmount = ClampNonNegative(ConsumeAmount);
	if (ConsumeAmount <= 0)
	{
		return true;
	}

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!AmmoStates.IsValidIndex(Index))
	{
		return false;
	}

	FAmmoState& State = AmmoStates[Index];
	if (State.MagAmmo < ConsumeAmount)
	{
		return false;
	}

	State.MagAmmo -= ConsumeAmount;
	State.MagAmmo = FMath::Clamp(State.MagAmmo, 0, State.MaxMag);

	OnAmmoChanged.Broadcast(WeaponType, State);
	return true;
}

FWeaponSlotState UMosesCombatComponent::GetWeaponSlot(EMosesWeaponType WeaponType) const
{
	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (WeaponSlots.IsValidIndex(Index))
	{
		return WeaponSlots[Index];
	}

	return FWeaponSlotState();
}

void UMosesCombatComponent::Server_SetWeaponSlot(EMosesWeaponType WeaponType, const FWeaponSlotState& NewSlot)
{
	check(GetOwner() && GetOwner()->HasAuthority());

	EnsureReplicatedArrays();
	InitializeDefaultsIfNeeded();

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!WeaponSlots.IsValidIndex(Index))
	{
		return;
	}

	WeaponSlots[Index] = NewSlot;

	// 서버도 즉시 브로드캐스트(리스닝 서버 HUD/로그)
	OnWeaponSlotChanged.Broadcast(WeaponType, NewSlot);
}

void UMosesCombatComponent::OnRep_AmmoStates()
{
	// 클라에서 RepNotify 도착 -> UI 갱신은 이벤트로만
	BroadcastAmmoAll();
}

void UMosesCombatComponent::OnRep_WeaponSlots()
{
	BroadcastSlotsAll();
}

void UMosesCombatComponent::InitializeDefaultsIfNeeded()
{
	if (bDefaultsInitialized)
	{
		return;
	}

	EnsureReplicatedArrays();

	// ✅ Day2 최소 기본값
	// - 유효한 WeaponType 인덱스에만 세팅
	// - enum 값이 바뀌면 여기도 함께 조정(또는 DataAsset로 이동)

	const auto TrySetAmmo = [this](EMosesWeaponType WeaponType, const FAmmoState& State)
		{
			const int32 Index = WeaponTypeToIndex(WeaponType);
			if (AmmoStates.IsValidIndex(Index))
			{
				AmmoStates[Index] = State;
			}
		};

	const auto TrySetSlot = [this](EMosesWeaponType WeaponType, const FWeaponSlotState& Slot)
		{
			const int32 Index = WeaponTypeToIndex(WeaponType);
			if (WeaponSlots.IsValidIndex(Index))
			{
				WeaponSlots[Index] = Slot;
			}
		};

	TrySetAmmo(EMosesWeaponType::Rifle, FAmmoState(/*Mag*/ 30, /*Reserve*/ 90, /*MaxMag*/ 30, /*MaxReserve*/ 180));
	TrySetAmmo(EMosesWeaponType::Pistol, FAmmoState(15, 45, 15, 90));
	TrySetAmmo(EMosesWeaponType::Melee, FAmmoState(0, 0, 0, 0));
	TrySetAmmo(EMosesWeaponType::Grenade, FAmmoState(1, 2, 1, 2));

	TrySetSlot(EMosesWeaponType::Rifle, FWeaponSlotState());
	TrySetSlot(EMosesWeaponType::Pistol, FWeaponSlotState());
	TrySetSlot(EMosesWeaponType::Melee, FWeaponSlotState());
	TrySetSlot(EMosesWeaponType::Grenade, FWeaponSlotState());

	bDefaultsInitialized = true;
}

void UMosesCombatComponent::EnsureReplicatedArrays()
{
	const int32 Count = GetWeaponTypeCount();
	if (Count <= 0)
	{
		return;
	}

	if (AmmoStates.Num() != Count)
	{
		AmmoStates.SetNum(Count);
	}

	if (WeaponSlots.Num() != Count)
	{
		WeaponSlots.SetNum(Count);
	}
}

int32 UMosesCombatComponent::GetWeaponTypeCount() const
{
	const UEnum* EnumObj = StaticEnum<EMosesWeaponType>();
	if (!EnumObj)
	{
		return 0;
	}

	// 대부분 프로젝트는 마지막에 MAX(또는 _MAX) 센티넬을 둔다.
	// Reflection 이름을 보고 마지막 항목이 MAX 계열이면 제외한다.
	const int32 Num = EnumObj->NumEnums();
	if (Num <= 0)
	{
		return 0;
	}

	const int32 LastIndex = Num - 1;
	const FString LastName = EnumObj->GetNameStringByIndex(LastIndex);

	// "MAX", "EMosesWeaponType_MAX", "EWhatever_MAX", "_MAX" 등 방어적으로 처리
	if (LastName.EndsWith(TEXT("MAX")) || LastName.EndsWith(TEXT("_MAX")))
	{
		return Num - 1;
	}

	// 숨김 항목이 없는 enum이면 그대로 사용
	return Num;
}

bool UMosesCombatComponent::IsValidWeaponType(EMosesWeaponType WeaponType) const
{
	const int32 Index = WeaponTypeToIndex(WeaponType);
	return Index >= 0 && Index < GetWeaponTypeCount();
}

int32 UMosesCombatComponent::WeaponTypeToIndex(EMosesWeaponType WeaponType) const
{
	return static_cast<int32>(WeaponType);
}

void UMosesCombatComponent::BroadcastAmmoAll() const
{
	const int32 Count = GetWeaponTypeCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!AmmoStates.IsValidIndex(Index))
		{
			continue;
		}

		const EMosesWeaponType WeaponType = static_cast<EMosesWeaponType>(Index);
		OnAmmoChanged.Broadcast(WeaponType, AmmoStates[Index]);
	}
}

void UMosesCombatComponent::BroadcastSlotsAll() const
{
	const int32 Count = GetWeaponTypeCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!WeaponSlots.IsValidIndex(Index))
		{
			continue;
		}

		const EMosesWeaponType WeaponType = static_cast<EMosesWeaponType>(Index);
		OnWeaponSlotChanged.Broadcast(WeaponType, WeaponSlots[Index]);
	}
}

int32 UMosesCombatComponent::ClampNonNegative(int32 Value)
{
	return FMath::Max(0, Value);
}
