#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"

#include "UE5_Multi_Shooter/GAS/MosesAbilitySet.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSet.h"

#include "MosesPlayerState.generated.h"

class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesSlotOwnershipComponent;
class UMosesAbilitySet;
class UMosesPawnData;
class UMosesCaptureComponent;
class UGameplayEffect;

// -----------------------------------------------------------------------------
// Native delegates (HUD = RepNotify/ASC Delegate -> Delegate only)
// -----------------------------------------------------------------------------
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesHealthChangedNative, float /*Cur*/, float /*Max*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesShieldChangedNative, float /*Cur*/, float /*Max*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesScoreChangedNative, int32 /*Score*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesDeathsChangedNative, int32 /*Deaths*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesGrenadeChangedNative, int32 /*Count*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedNative, int32 /*SelectedId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPlayerCapturesChangedNative, int32 /*Captures*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPlayerZombieKillsChangedNative, int32 /*ZombieKills*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPlayerPvPKillsChangedNative, int32 /*PvPKills*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPlayerHeadshotsChangedNative, int32 /*Headshots*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesDeathStateChangedNative, bool /*bIsDead*/, float /*RespawnEndServerTime*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedBP, int32, SelectedId);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTotalKillsChangedNative, int32 /*TotalKills*/);


UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState(const FObjectInitializer& ObjectInitializer);

	//~AActor
	virtual void PostInitializeComponents() override;

	//~APlayerState
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;
	virtual void OnRep_Score() override;

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

public:
	// ---------------------------------------------------------------------
	// Getters
	// ---------------------------------------------------------------------
	const FGuid& GetPersistentId() const { return PersistentId; }
	const FString& GetPlayerNickName() const { return PlayerNickName; }

	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }
	bool IsRoomHost() const { return bIsRoomHost; }
	const FGuid& GetRoomId() const { return RoomId; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }

	template<typename TPawnData>
	const TPawnData* GetPawnData() const { return Cast<TPawnData>(PawnData); }

	UMosesSlotOwnershipComponent* GetSlotOwnershipComponent() const { return SlotOwnershipComponent; }
	UMosesCombatComponent* GetCombatComponent() const { return CombatComponent; }

	int32 GetDeaths() const { return Deaths; }
	int32 GetCaptures() const { return Captures; }
	int32 GetZombieKills() const { return ZombieKills; }
	int32 GetPvPKills() const { return PvPKills; }
	int32 GetHeadshots() const { return Headshots; }
	bool IsDead() const { return bIsDead; }
	float GetRespawnEndServerTime() const { return RespawnEndServerTime; }
	int32 GetTotalKills() const { return PvPKills + ZombieKills; }

public:
	// ---------------------------------------------------------------------
	// GAS Init
	// ---------------------------------------------------------------------
	void TryInitASC(AActor* InAvatarActor);

public:
	// ---------------------------------------------------------------------
	// DAY11 [MOD] Death notify from AttributeSet
	// - AttributeSet(PostGameplayEffectExecute)에서 Health<=0이면 이 함수를 호출한다.
	// - 서버에서만 동작해야 하며, 중복 호출에 안전해야 한다.
	// ---------------------------------------------------------------------
	void ServerNotifyDeathFromGAS(); // [MOD]

public:
	// ---------------------------------------------------------------------
	// DAY11 [MOD] Shield Regen (Armor Regen)
	// - 15초마다 +1 Shield (GE_ShieldRegen_One 적용)
	// ---------------------------------------------------------------------
	void ServerStartShieldRegen(); // [MOD]
	void ServerStopShieldRegen();  // [MOD]

	UPROPERTY(EditDefaultsOnly, Category="Moses|GAS")
	TSubclassOf<UGameplayEffect> GE_ShieldRegen_One; // [MOD]

public:
	// ---------------------------------------------------------------------
	// Match default loadout (Server only)
	// ---------------------------------------------------------------------
	void ServerEnsureMatchDefaultLoadout();

	// GF_Combat_GAS: AbilitySet apply (Server only)
	void ServerApplyCombatAbilitySetOnce(UMosesAbilitySet* InAbilitySet);

public:
	// ---------------------------------------------------------------------
	// Lobby / Player info (Server authority)
	// ---------------------------------------------------------------------
	void EnsurePersistentId_Server();
	void ServerSetLoggedIn(bool bInLoggedIn);
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacterId(int32 InId);

	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);
	void ServerSetPlayerNickName(const FString& InNickName);
	void ServerAddDeath();

public:
	// Score SSOT
	void ServerAddScore(int32 Delta, const TCHAR* Reason);

	// Match stats (Server authority, SSOT=PlayerState)
	void ServerAddCapture(int32 Delta = 1);
	void ServerAddZombieKill(int32 Delta = 1);

	void ServerAddPvPKill(int32 Delta = 1);
	void ServerAddHeadshot(int32 Delta = 1);
	void ServerClearDeadAfterRespawn();

public:
	// ---------------------------------------------------------------------
	// RepNotifies
	// ---------------------------------------------------------------------
	UFUNCTION()
	void OnRep_PersistentId();

	UFUNCTION()
	void OnRep_LoggedIn();

	UFUNCTION()
	void OnRep_Ready();

	UFUNCTION()
	void OnRep_SelectedCharacterId();

	UFUNCTION()
	void OnRep_Room();

	UFUNCTION()
	void OnRep_PlayerNickName();

	UFUNCTION()
	void OnRep_Deaths();

	UFUNCTION()
	void OnRep_Captures();

	UFUNCTION()
	void OnRep_ZombieKills();

	UFUNCTION()
	void OnRep_PvPKills();

	UFUNCTION()
	void OnRep_Headshots();

	UFUNCTION()
	void OnRep_DeathState();

public:
	// ---------------------------------------------------------------------
	// Native Delegates (HUD Update)
	// ---------------------------------------------------------------------
	FOnMosesHealthChangedNative OnHealthChanged;
	FOnMosesShieldChangedNative OnShieldChanged;
	FOnMosesScoreChangedNative OnScoreChanged;
	FOnMosesDeathsChangedNative OnDeathsChanged;
	FOnMosesAmmoChangedNative OnAmmoChanged;
	FOnMosesGrenadeChangedNative OnGrenadeChanged;
	FOnMosesPlayerCapturesChangedNative OnPlayerCapturesChanged;
	FOnMosesPlayerZombieKillsChangedNative OnPlayerZombieKillsChanged;
	FOnMosesPlayerPvPKillsChangedNative OnPlayerPvPKillsChanged;
	FOnMosesPlayerHeadshotsChangedNative OnPlayerHeadshotsChanged;
	FOnMosesDeathStateChangedNative OnDeathStateChanged;
	FOnMosesSelectedCharacterChangedNative OnSelectedCharacterChangedNative;
	FOnTotalKillsChangedNative OnTotalKillsChanged;

	UPROPERTY(BlueprintAssignable)
	FOnMosesSelectedCharacterChangedBP OnSelectedCharacterChangedBP;

public:
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

private:
	// Internals
	void NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const;

	void BindASCAttributeDelegates();
	void BroadcastVitals_Initial();
	void BroadcastScore();
	void BroadcastDeaths();
	void BroadcastAmmoAndGrenade();
	void BroadcastSelectedCharacterChanged(const TCHAR* Reason);
	void BroadcastPlayerCaptures();
	void BroadcastPlayerZombieKills();
	void BroadcastPlayerPvPKills();
	void BroadcastPlayerHeadshots();
	void BroadcastDeathState();
	void BroadcastTotalKills();

	// GAS Attribute callbacks
	void HandleHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleShieldChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged_Internal(const FOnAttributeChangeData& Data);



private:
	// Components (SSOT)
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesAttributeSet> AttributeSet = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesCombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesSlotOwnershipComponent> SlotOwnershipComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCaptureComponent> CaptureComponent = nullptr;

private:
	// GAS runtime state
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedAvatar;

	bool bASCInitialized = false;
	bool bASCDelegatesBound = false;

	bool bCombatAbilitySetApplied = false;

	UPROPERTY()
	bool bMatchDefaultLoadoutGranted = false;

	FMosesAbilitySet_GrantedHandles CombatAbilitySetHandles;

private:
	// ---------------------------------------------------------------------
	// Replicated data (Lobby + Match 공용)
	// ---------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_PlayerNickName)
	FString PlayerNickName;

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

	UPROPERTY(Replicated)
	bool bIsRoomHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths)
	int32 Deaths = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Captures)
	int32 Captures = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieKills)
	int32 ZombieKills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_PvPKills)
	int32 PvPKills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Headshots)
	int32 Headshots = 0;

	// Death/Respawn replicated state
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	float RespawnEndServerTime = 0.f;

	// Shield regen timer
	FTimerHandle TimerHandle_ShieldRegen;

	// PawnData
	UPROPERTY()
	TObjectPtr<UObject> PawnData = nullptr;
};
