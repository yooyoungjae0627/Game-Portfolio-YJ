#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h" // FOnAttributeChangeData

// ✅ [FIX] 전역 타입 FMosesAbilitySet_GrantedHandles 정의가 여기 있음
#include "UE5_Multi_Shooter/GAS/MosesAbilitySet.h"

#include "MosesPlayerState.generated.h"

class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesSlotOwnershipComponent;
class UMosesAbilitySet;
class UMosesPawnData;

// -----------------------------------------------------------------------------
// Native delegates (HUD = RepNotify -> Delegate only)
// -----------------------------------------------------------------------------
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesHealthChangedNative, float /*Cur*/, float /*Max*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesShieldChangedNative, float /*Cur*/, float /*Max*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesScoreChangedNative, int32 /*Score*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesDeathsChangedNative, int32 /*Deaths*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesGrenadeChangedNative, int32 /*Count*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedNative, int32 /*SelectedId*/);

// BP delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedBP, int32, SelectedId);

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState(const FObjectInitializer& ObjectInitializer);

	//~AActor interface
	virtual void PostInitializeComponents() override;

	//~APlayerState interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;
	virtual void OnRep_Score() override;

	//~IAbilitySystemInterface interface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

public:
	// ---------------------------------------------------------------------
	// Getters (프로젝트 전역 호환)
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

public:
	// ---------------------------------------------------------------------
	// GAS Init
	// ---------------------------------------------------------------------
	void TryInitASC(AActor* InAvatarActor);

public:
	// ---------------------------------------------------------------------
	// [MOD] Match default loadout (Server only)
	// - MatchLevel 진입 시 기본 Rifle + 30/90 보장
	// ---------------------------------------------------------------------
	void ServerEnsureMatchDefaultLoadout();

	// ---------------------------------------------------------------------
	// [MOD] GF_Combat_GAS: AbilitySet apply (Server only)
	// ---------------------------------------------------------------------
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

	FOnMosesSelectedCharacterChangedNative OnSelectedCharacterChangedNative;

	UPROPERTY(BlueprintAssignable)
	FOnMosesSelectedCharacterChangedBP OnSelectedCharacterChangedBP;

public:
	// ---------------------------------------------------------------------
	// DoD log (외부에서 호출됨 → public)
	// ---------------------------------------------------------------------
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

private:
	// ---------------------------------------------------------------------
	// Internals
	// ---------------------------------------------------------------------
	void NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const;

	void BindASCAttributeDelegates();
	void BroadcastVitals_Initial();
	void BroadcastScore();
	void BroadcastDeaths();
	void BroadcastAmmoAndGrenade();
	void BroadcastSelectedCharacterChanged(const TCHAR* Reason);

	// GAS Attribute callbacks
	void HandleHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleShieldChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged_Internal(const FOnAttributeChangeData& Data);

private:
	// ---------------------------------------------------------------------
	// Components (SSOT)
	// ---------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesAttributeSet> AttributeSet = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesCombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMosesSlotOwnershipComponent> SlotOwnershipComponent = nullptr;

private:
	// ---------------------------------------------------------------------
	// GAS runtime state
	// ---------------------------------------------------------------------
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedAvatar;

	bool bASCInitialized = false;
	bool bASCDelegatesBound = false;

	// [MOD] AbilitySet apply once
	bool bCombatAbilitySetApplied = false;

	// [MOD] Match default loadout once (중복 호출 안전)
	UPROPERTY()
	bool bMatchDefaultLoadoutGranted = false;

	// ✅ [FIX] 전역 타입 그대로 (nested/forward 금지)
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

	// PawnData는 외부가 GetPawnData<T>()로 조회
	UPROPERTY()
	TObjectPtr<UObject> PawnData = nullptr;
};
