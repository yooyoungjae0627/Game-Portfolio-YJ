// ============================================================================
// MosesCombatComponent.cpp
// ============================================================================

#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesCombatComponent::UMosesCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	EnsureArraysSized(); // [ADD] 배열 크기 보장(서버/클라 모두)
}

void UMosesCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서 Day2 기본값 확정(SSOT)
	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Server_EnsureInitialized_Day2(); // [ADD]
		}
	}
}

void UMosesCombatComponent::Server_EnsureInitialized_Day2()
{
	AActor* Owner = GetOwner();
	check(Owner && Owner->HasAuthority());

	if (bServerInitialized_Day2)
	{
		return;
	}

	EnsureArraysSized();

	// ---------------------------
	// Ammo defaults (Rifle/Pistol만 최소)
	// ---------------------------
	{
		const int32 RifleIdx = WeaponTypeToIndex(EMosesWeaponType::Rifle);
		AmmoStates[RifleIdx] = FAmmoState(DefaultRifleMag, DefaultRifleReserve, DefaultRifleMaxMag, DefaultRifleMaxReserve);

		const int32 PistolIdx = WeaponTypeToIndex(EMosesWeaponType::Pistol);
		AmmoStates[PistolIdx] = FAmmoState(DefaultPistolMag, DefaultPistolReserve, DefaultPistolMaxMag, DefaultPistolMaxReserve);

		// Melee/Grenade는 프로젝트 정책에 맞게 채우면 됨(일단 0)
		const int32 MeleeIdx = WeaponTypeToIndex(EMosesWeaponType::Melee);
		AmmoStates[MeleeIdx] = FAmmoState(0, 0, 0, 0);

		const int32 GrenadeIdx = WeaponTypeToIndex(EMosesWeaponType::Grenade);
		AmmoStates[GrenadeIdx] = FAmmoState(0, 0, 0, 0);
	}

	// ---------------------------
	// Weapon slot defaults
	// ---------------------------
	{
		WeaponSlots[WeaponTypeToIndex(EMosesWeaponType::Rifle)].WeaponType = EMosesWeaponType::Rifle;
		WeaponSlots[WeaponTypeToIndex(EMosesWeaponType::Pistol)].WeaponType = EMosesWeaponType::Pistol;
		WeaponSlots[WeaponTypeToIndex(EMosesWeaponType::Melee)].WeaponType = EMosesWeaponType::Melee;
		WeaponSlots[WeaponTypeToIndex(EMosesWeaponType::Grenade)].WeaponType = EMosesWeaponType::Grenade;

		// WeaponId 기본값은 0으로 되어있던데(네 구조), “미장착”이면 0 유지 or INDEX_NONE로 바꿔도 됨.
	}

	bServerInitialized_Day2 = true;

	ForceReplication(); // [FIX] 기존 ForceReplication() 식별자 에러 해결
	UE_LOG(LogMosesPlayer, Warning, TEXT("[Combat][SV] Day2 Defaults Initialized Owner=%s"), *GetNameSafe(Owner));

	BroadcastCombatDataChanged(TEXT("ServerInit_Day2")); // 리슨 서버 UI/로그 즉시 갱신
}

void UMosesCombatComponent::Server_SetAmmoState(EMosesWeaponType WeaponType, const FAmmoState& NewState)
{
	AActor* Owner = GetOwner();
	check(Owner && Owner->HasAuthority());

	Server_EnsureInitialized_Day2();

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!AmmoStates.IsValidIndex(Index))
	{
		return;
	}

	FAmmoState& State = AmmoStates[Index];
	State = NewState;

	// 안전 클램프
	State.MaxMag = FMath::Max(0, State.MaxMag);
	State.MaxReserve = FMath::Max(0, State.MaxReserve);
	State.MagAmmo = FMath::Clamp(State.MagAmmo, 0, State.MaxMag);
	State.ReserveAmmo = FMath::Clamp(State.ReserveAmmo, 0, State.MaxReserve);

	ForceReplication();

	UE_LOG(LogMosesPlayer, Log, TEXT("[Combat][SV] SetAmmoState Type=%s Mag=%d/%d Res=%d/%d Owner=%s"),
		*UEnum::GetValueAsString(WeaponType),
		State.MagAmmo, State.MaxMag,
		State.ReserveAmmo, State.MaxReserve,
		*GetNameSafe(Owner));

	BroadcastCombatDataChanged(TEXT("Server_SetAmmoState"));
}

void UMosesCombatComponent::Server_AddReserveAmmo(EMosesWeaponType WeaponType, int32 DeltaReserve)
{
	AActor* Owner = GetOwner();
	check(Owner && Owner->HasAuthority());

	Server_EnsureInitialized_Day2();

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!AmmoStates.IsValidIndex(Index))
	{
		return;
	}

	FAmmoState& State = AmmoStates[Index];
	State.ReserveAmmo = FMath::Clamp(State.ReserveAmmo + DeltaReserve, 0, State.MaxReserve);

	ForceReplication();

	UE_LOG(LogMosesPickup, Log, TEXT("[Combat][SV] AddReserveAmmo Type=%s Delta=%d Res=%d/%d Owner=%s"),
		*UEnum::GetValueAsString(WeaponType),
		DeltaReserve,
		State.ReserveAmmo, State.MaxReserve,
		*GetNameSafe(Owner));

	BroadcastCombatDataChanged(TEXT("Server_AddReserveAmmo"));
}

void UMosesCombatComponent::Server_AddMagAmmo(EMosesWeaponType WeaponType, int32 DeltaMag)
{
	AActor* Owner = GetOwner();
	check(Owner && Owner->HasAuthority());

	Server_EnsureInitialized_Day2();

	const int32 Index = WeaponTypeToIndex(WeaponType);
	if (!AmmoStates.IsValidIndex(Index))
	{
		return;
	}

	FAmmoState& State = AmmoStates[Index];
	State.MagAmmo = FMath::Clamp(State.MagAmmo + DeltaMag, 0, State.MaxMag);

	ForceReplication();
	BroadcastCombatDataChanged(TEXT("Server_AddMagAmmo"));
}

void UMosesCombatComponent::Server_SetWeaponSlot(EMosesWeaponType SlotType, const FWeaponSlotState& NewState)
{
	AActor* Owner = GetOwner();
	check(Owner && Owner->HasAuthority());

	Server_EnsureInitialized_Day2();

	const int32 Index = WeaponTypeToIndex(SlotType);
	if (!WeaponSlots.IsValidIndex(Index))
	{
		return;
	}

	WeaponSlots[Index] = NewState;
	WeaponSlots[Index].WeaponType = SlotType; // [ADD] 서버에서 강제 일관성

	ForceReplication();

	UE_LOG(LogMosesPlayer, Log, TEXT("[Combat][SV] SetSlot Slot=%s WeaponId=%d Owner=%s"),
		*UEnum::GetValueAsString(SlotType),
		WeaponSlots[Index].WeaponId,
		*GetNameSafe(Owner));

	BroadcastCombatDataChanged(TEXT("Server_SetWeaponSlot"));
}

void UMosesCombatComponent::OnRep_AmmoStates()
{
	BroadcastCombatDataChanged(TEXT("OnRep_AmmoStates"));
}

void UMosesCombatComponent::OnRep_WeaponSlots()
{
	BroadcastCombatDataChanged(TEXT("OnRep_WeaponSlots"));
}

void UMosesCombatComponent::OnRep_ServerInitialized_Day2()
{
	BroadcastCombatDataChanged(TEXT("OnRep_ServerInitialized_Day2"));
}

void UMosesCombatComponent::BroadcastCombatDataChanged(const TCHAR* Reason)
{
	OnCombatDataChangedNative.Broadcast(Reason);

	const FString ReasonStr = Reason ? FString(Reason) : FString(TEXT("Unknown"));
	OnCombatDataChangedBP.Broadcast(ReasonStr);

	UE_LOG(LogMosesPlayer, Verbose, TEXT("[Combat] DataChanged Reason=%s Owner=%s"),
		*ReasonStr, *GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::EnsureArraysSized()
{
	// EMosesWeaponType 은 Max 가 없으니, 슬롯 수를 “정의된 개수”로 맞춘다.
	// 현재 enum: Rifle/Pistol/Melee/Grenade (4개)
	constexpr int32 WeaponTypeCount = 4; // [ADD] 필요하면 여기만 수정

	if (AmmoStates.Num() != WeaponTypeCount)
	{
		AmmoStates.SetNum(WeaponTypeCount);
	}

	if (WeaponSlots.Num() != WeaponTypeCount)
	{
		WeaponSlots.SetNum(WeaponTypeCount);
	}
}

int32 UMosesCombatComponent::WeaponTypeToIndex(EMosesWeaponType WeaponType) const
{
	// 현재 enum 순서가 0..3 이어야 한다는 전제.
	// (Rifle=0, Pistol=1, Melee=2, Grenade=3)
	return static_cast<int32>(WeaponType);
}

void UMosesCombatComponent::ForceReplication()
{
	// [ADD] UActorComponent에는 ForceReplication()이 없음.
	// 가장 간단하고 안전한 방식: Owner의 NetUpdate를 강제로 올려서 Replication flush를 유도.
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UMosesCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesCombatComponent, AmmoStates);
	DOREPLIFETIME(UMosesCombatComponent, WeaponSlots);
	DOREPLIFETIME(UMosesCombatComponent, bServerInitialized_Day2);
}
