#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesWeaponTypes.h"
#include "MosesCombatComponent.generated.h"

// ---------------------------
// Delegates (UI/HUD bind)
// ---------------------------
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesCombatDataChangedNative, const TCHAR* /*Reason*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMosesCombatDataChangedBP, FString, Reason);

/**
 * UMosesCombatComponent
 *
 * [정책]
 * - PlayerState 소유 (SSOT)
 * - 서버에서만 초기값/변경 확정
 * - 클라는 RepNotify -> Delegate로 UI 갱신 (Tick 금지)
 */
UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	// ------------------------------------------------------------
	// Getters (UI/HUD)
	// ------------------------------------------------------------
	const TArray<FAmmoState>& GetAmmoStates() const { return AmmoStates; }
	const TArray<FWeaponSlotState>& GetWeaponSlots() const { return WeaponSlots; }
	bool IsServerInitialized_Day2() const { return bServerInitialized_Day2; }

public:
	// ------------------------------------------------------------
	// Delegates (UI bind)
	// ------------------------------------------------------------
	FOnMosesCombatDataChangedNative OnCombatDataChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "Moses|Combat")
	FOnMosesCombatDataChangedBP OnCombatDataChangedBP;

public:
	// ------------------------------------------------------------
	// Server-only init / server authoritative setters
	// (서버에서만 호출한다는 전제 - RPC 아님)
	// ------------------------------------------------------------
	void Server_EnsureInitialized_Day2();

	void Server_SetAmmoState(EMosesWeaponType WeaponType, const FAmmoState& NewState);
	void Server_AddReserveAmmo(EMosesWeaponType WeaponType, int32 DeltaReserve);
	void Server_AddMagAmmo(EMosesWeaponType WeaponType, int32 DeltaMag);
	void Server_SetWeaponSlot(EMosesWeaponType SlotType, const FWeaponSlotState& NewState);

protected:
	virtual void BeginPlay() override;

private:
	// ------------------------------------------------------------
	// RepNotifies
	// ------------------------------------------------------------
	UFUNCTION()
	void OnRep_AmmoStates();

	UFUNCTION()
	void OnRep_WeaponSlots();

	UFUNCTION()
	void OnRep_ServerInitialized_Day2();

private:
	// ------------------------------------------------------------
	// Internal helpers
	// ------------------------------------------------------------
	void BroadcastCombatDataChanged(const TCHAR* Reason);
	void EnsureArraysSized();
	int32 WeaponTypeToIndex(EMosesWeaponType WeaponType) const;

	void ForceReplication();

private:
	// ------------------------------------------------------------
	// Replication
	// ------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ------------------------------------------------------------
	// Replicated data
	// ------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_AmmoStates)
	TArray<FAmmoState> AmmoStates;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponSlots)
	TArray<FWeaponSlotState> WeaponSlots;

	UPROPERTY(ReplicatedUsing = OnRep_ServerInitialized_Day2)
	bool bServerInitialized_Day2 = false;

private:
	// ------------------------------------------------------------
	// Tuning (server defaults) - 필요 최소만
	// ------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultRifleMag = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultRifleReserve = 90;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultRifleMaxMag = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultRifleMaxReserve = 120;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultPistolMag = 15;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultPistolReserve = 45;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultPistolMaxMag = 15;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Combat|Defaults")
	int32 DefaultPistolMaxReserve = 60;
};
