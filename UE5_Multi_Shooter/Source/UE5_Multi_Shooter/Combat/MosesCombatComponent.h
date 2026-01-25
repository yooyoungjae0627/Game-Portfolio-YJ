#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/Combat/MosesWeaponTypes.h"

#include "MosesCombatComponent.generated.h"

/**
 * UMosesCombatComponent
 *
 * [역할]
 * - 전투 상태(Ammo / WeaponSlot)를 서버 권위로 관리
 * - 반드시 PlayerState 소유
 * - Pawn에는 절대 상태를 두지 않는다
 */

DECLARE_MULTICAST_DELEGATE_OneParam(FMosesCombatDataChangedNative, const TCHAR* /*Reason*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMosesCombatDataChangedBP, const FString&, Reason);

UCLASS(ClassGroup = (Moses), meta = (BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesCombatComponent(const FObjectInitializer& ObjectInitializer);

public:
	const TArray<FAmmoState>& GetAmmoStates() const { return AmmoStates; }
	const TArray<FWeaponSlotState>& GetWeaponSlots() const { return WeaponSlots; }

	// ----------------------------
	// Server-only API
	// ----------------------------
	void Server_EnsureInitialized_Day2();
	void Server_SetAmmoState(EMosesWeaponType WeaponType, const FAmmoState& NewState);
	void Server_AddReserveAmmo(EMosesWeaponType WeaponType, int32 DeltaReserve);
	void Server_AddMagAmmo(EMosesWeaponType WeaponType, int32 DeltaMag);
	void Server_SetWeaponSlot(EMosesWeaponType SlotType, const FWeaponSlotState& NewState);

	// ----------------------------
	// Delegates
	// ----------------------------
	FMosesCombatDataChangedNative OnCombatDataChangedNative;

	UPROPERTY(BlueprintAssignable)
	FMosesCombatDataChangedBP OnCombatDataChangedBP;

protected:
	virtual void BeginPlay() override;

private:
	// ----------------------------
	// Internal helpers
	// ----------------------------
	void BroadcastCombatDataChanged(const TCHAR* Reason);
	void EnsureArraysSized();
	int32 WeaponTypeToIndex(EMosesWeaponType WeaponType) const;
	void ForceReplication();
	void ValidateOwnerIsPlayerState() const;

private:
	// ----------------------------
	// Replicated states
	// ----------------------------
	UPROPERTY(ReplicatedUsing = OnRep_AmmoStates)
	TArray<FAmmoState> AmmoStates;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponSlots)
	TArray<FWeaponSlotState> WeaponSlots;

	UPROPERTY(ReplicatedUsing = OnRep_ServerInitialized_Day2)
	bool bServerInitialized_Day2 = false;

	// ----------------------------
	// ★ 누락돼서 에러 났던 부분 (필수)
	// ----------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultRifleMag = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultRifleReserve = 90;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultRifleMaxMag = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultRifleMaxReserve = 180;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultPistolMag = 12;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultPistolReserve = 24;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultPistolMaxMag = 12;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Day2|Defaults")
	int32 DefaultPistolMaxReserve = 60;

private:
	UFUNCTION()
	void OnRep_AmmoStates();

	UFUNCTION()
	void OnRep_WeaponSlots();

	UFUNCTION()
	void OnRep_ServerInitialized_Day2();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
