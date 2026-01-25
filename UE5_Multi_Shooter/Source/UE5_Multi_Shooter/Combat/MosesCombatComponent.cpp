#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

// Owner가 PlayerState인지 검사하기 위해 포함
#include "GameFramework/PlayerState.h"

UMosesCombatComponent::UMosesCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	EnsureArraysSized();
}

void UMosesCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// [ADD] CombatComponent는 무조건 PlayerState 소유여야 한다.
	ValidateOwnerIsPlayerState();

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Server_EnsureInitialized_Day2();
		}
	}
}

void UMosesCombatComponent::ValidateOwnerIsPlayerState() const
{
	const AActor* Owner = GetOwner();
	const bool bOk = (Owner != nullptr) && Owner->IsA(APlayerState::StaticClass());

	ensureAlwaysMsgf(
		bOk,
		TEXT("[COMBAT] CombatComponent must be owned by PlayerState. Owner=%s Class=%s"),
		*GetNameSafe(Owner),
		Owner ? *Owner->GetClass()->GetName() : TEXT("None"));

	if (!bOk)
	{
		UE_LOG(LogMosesCombat, Error, TEXT("%s CombatComponent Owner INVALID (Must be PlayerState). Owner=%s"),
			MOSES_TAG_COMBAT_SV,
			*GetNameSafe(Owner));
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
	}

	bServerInitialized_Day2 = true;

	ForceReplication();

	UE_LOG(LogMosesCombat, Warning, TEXT("%s Day2 Defaults Initialized Owner=%s"),
		MOSES_TAG_COMBAT_SV,
		*GetNameSafe(Owner));

	BroadcastCombatDataChanged(TEXT("ServerInit_Day2"));
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

	State.MaxMag = FMath::Max(0, State.MaxMag);
	State.MaxReserve = FMath::Max(0, State.MaxReserve);
	State.MagAmmo = FMath::Clamp(State.MagAmmo, 0, State.MaxMag);
	State.ReserveAmmo = FMath::Clamp(State.ReserveAmmo, 0, State.MaxReserve);

	ForceReplication();

	UE_LOG(LogMosesCombat, Log, TEXT("%s SetAmmoState Type=%s Mag=%d/%d Res=%d/%d Owner=%s"),
		MOSES_TAG_COMBAT_SV,
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

	UE_LOG(LogMosesPickup, Log, TEXT("%s AddReserveAmmo Type=%s Delta=%d Res=%d/%d Owner=%s"),
		MOSES_TAG_COMBAT_SV,
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

	UE_LOG(LogMosesCombat, Verbose, TEXT("%s AddMagAmmo Type=%s Delta=%d Mag=%d/%d Owner=%s"),
		MOSES_TAG_COMBAT_SV,
		*UEnum::GetValueAsString(WeaponType),
		DeltaMag,
		State.MagAmmo, State.MaxMag,
		*GetNameSafe(Owner));

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
	WeaponSlots[Index].WeaponType = SlotType;

	ForceReplication();

	UE_LOG(LogMosesCombat, Log, TEXT("%s SetSlot Slot=%s WeaponId=%d Owner=%s"),
		MOSES_TAG_COMBAT_SV,
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

	UE_LOG(LogMosesCombat, Verbose, TEXT("[COMBAT] DataChanged Reason=%s Owner=%s"),
		*ReasonStr, *GetNameSafe(GetOwner()));
}

void UMosesCombatComponent::EnsureArraysSized()
{
	constexpr int32 WeaponTypeCount = 4;

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
	return static_cast<int32>(WeaponType);
}

void UMosesCombatComponent::ForceReplication()
{
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
