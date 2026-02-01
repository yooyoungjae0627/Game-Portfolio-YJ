#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"

#include "UE5_Multi_Shooter/GAS/MosesAbilitySet.h" // ✅ [MOD] Handles 멤버를 위해 포함

#include "MosesPlayerState.generated.h"

class UAbilitySystemComponent;
class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesPawnData;
class UMosesLobbyLocalPlayerSubsystem;

class UMosesSlotOwnershipComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedNative, int32 /*NewId*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedBP, int32, NewId);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesVitalChangedNative, float /*Current*/, float /*Max*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesIntChangedNative, int32 /*Value*/);

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;
	virtual void OnRep_Score() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	void TryInitASC(AActor* InAvatarActor);

	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }

	const FGuid& GetPersistentId() const { return PersistentId; }
	const FGuid& GetRoomId() const { return RoomId; }
	bool IsRoomHost() const { return bIsRoomHost; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }
	const FString& GetPlayerNickName() const { return PlayerNickName; }

	UMosesCombatComponent* GetCombatComponent() const { return CombatComponent; }
	const UMosesAttributeSet* GetMosesAttributeSet() const { return AttributeSet; }

	UMosesSlotOwnershipComponent* GetSlotOwnershipComponent() const { return SlotOwnershipComponent; }

	// ---------------------------------------------------------------------
	// ✅ [MOD] GF_Combat_GAS: AbilitySet apply (Server only)
	// ---------------------------------------------------------------------
	void ServerApplyCombatAbilitySetOnce(UMosesAbilitySet* InAbilitySet);

	// ---------------------------------------------------------------------
	// ✅ [MOD] Match default loadout (Server only)
	// ---------------------------------------------------------------------
	void ServerGrantDefaultMatchLoadoutIfNeeded();

	// Lobby delegates
	FOnMosesSelectedCharacterChangedNative OnSelectedCharacterChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "Moses|Lobby")
	FOnMosesSelectedCharacterChangedBP OnSelectedCharacterChangedBP;

	// HUD delegates
	FOnMosesVitalChangedNative OnHealthChanged;
	FOnMosesVitalChangedNative OnShieldChanged;

	FOnMosesIntChangedNative OnScoreChanged;
	FOnMosesIntChangedNative OnDeathsChanged;

	FOnMosesAmmoChangedNative OnAmmoChanged;
	FOnMosesIntChangedNative OnGrenadeChanged;

	// Server setters (existing)
	void EnsurePersistentId_Server();
	void ServerSetLoggedIn(bool bInLoggedIn);
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacterId(int32 InId);

	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);
	void ServerSetPlayerNickName(const FString& InNickName);

	template <typename TPawnData>
	const TPawnData* GetPawnData() const { return Cast<TPawnData>(PawnData); }
	void SetPawnData(const UMosesPawnData* InPawnData) { PawnData = InPawnData; }

	void ServerAddDeath();
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

private:
	UFUNCTION() void OnRep_PersistentId();
	UFUNCTION() void OnRep_LoggedIn();
	UFUNCTION() void OnRep_Ready();
	UFUNCTION() void OnRep_SelectedCharacterId();
	UFUNCTION() void OnRep_Room();
	UFUNCTION() void OnRep_PlayerNickName();
	UFUNCTION() void OnRep_Deaths();

	void BroadcastSelectedCharacterChanged(const TCHAR* Reason);
	void NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const;

	void BindCombatDelegatesOnce();

	UFUNCTION()
	void HandleCombatDataChanged_BP(const FString& Reason);

	void HandleCombatDataChanged_Native(const TCHAR* Reason);

	void BroadcastAmmoAndGrenade();
	void BroadcastScore();
	void BroadcastDeaths();

	void BindASCAttributeDelegates();
	void BroadcastVitals_Initial();

	void HandleHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleShieldChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged_Internal(const FOnAttributeChangeData& Data);

private:
	UPROPERTY(ReplicatedUsing = OnRep_PersistentId)
	FGuid PersistentId;

	UPROPERTY(ReplicatedUsing = OnRep_LoggedIn)
	bool bLoggedIn = false;

	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId)
	int32 SelectedCharacterId = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Room)
	FGuid RoomId;

	UPROPERTY(ReplicatedUsing = OnRep_Room)
	bool bIsRoomHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerNickName)
	FString PlayerNickName;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths)
	int32 Deaths = 0;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UMosesPawnData> PawnData = nullptr;

private:
	UPROPERTY(VisibleAnywhere, Category = "Moses|Combat")
	TObjectPtr<UMosesCombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|GAS")
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesAttributeSet> AttributeSet = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Pickup")
	TObjectPtr<UMosesSlotOwnershipComponent> SlotOwnershipComponent = nullptr;

private:
	// ---------------------------------------------------------------------
	// ✅ [MOD] AbilitySet apply guards (per-player)
	// ---------------------------------------------------------------------
	UPROPERTY(Transient)
	bool bCombatAbilitySetApplied = false;

	/** ✅ [MOD] 전역 static 제거: PlayerState 인스턴스에 핸들을 보관 */
	UPROPERTY(Transient)
	FMosesAbilitySet_GrantedHandles CombatAbilitySetHandles;

private:
	UPROPERTY(Transient)
	bool bASCInitialized = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedAvatar;

	UPROPERTY(Transient)
	bool bCombatDelegatesBound = false;

	UPROPERTY(Transient)
	bool bASCDelegatesBound = false;
};
