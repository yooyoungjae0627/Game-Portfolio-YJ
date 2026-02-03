#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "UE5_Multi_Shooter/GAS/MosesAbilitySet.h"
#include "UE5_Multi_Shooter/GAS/AttributeSet/MosesAttributeSetBase.h"
#include "MosesPlayerState.generated.h"

class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesSlotOwnershipComponent;
class UMosesAbilitySet;
class UMosesPawnData;
class UMosesCaptureComponent;

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

// ✅ [FIX][MOD] 이름 충돌 방지
// - MosesCaptureComponent.h 에 이미 "FOnMosesCapturesChangedNative (TwoParams)"가 존재하므로
//   PlayerState/HUD 전용 델리게이트는 고유 이름으로 분리한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPlayerCapturesChangedNative, int32 /*Captures*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesPlayerZombieKillsChangedNative, int32 /*ZombieKills*/);

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

	// ✅ HUD에서 호출하는 게터 제공 (직접 변수 접근 금지)
	int32 GetDeaths() const { return Deaths; }
	int32 GetCaptures() const { return Captures; }
	int32 GetZombieKills() const { return ZombieKills; }

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
	// [SCORE][SV] Score를 서버에서 누적한다. (SSOT)
	// - HUD는 OnScoreChanged(int32) 델리게이트만 구독한다.
	// ---------------------------------------------------------------------
	void ServerAddScore(int32 Delta, const TCHAR* Reason);

public:
	// ---------------------------------------------------------------------
	// [MOD] Match stats (Server authority, SSOT=PlayerState)
	// - Capture 성공 시: ServerAddCapture()
	// - Zombie Kill 확정 시: ServerAddZombieKill()
	// ---------------------------------------------------------------------
	void ServerAddCapture(int32 Delta = 1);
	void ServerAddZombieKill(int32 Delta = 1);

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

	// [MOD]
	UFUNCTION()
	void OnRep_Captures();

	// [MOD]
	UFUNCTION()
	void OnRep_ZombieKills();

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

	// ✅ [FIX][MOD] 충돌 방지 리네임
	FOnMosesPlayerCapturesChangedNative OnPlayerCapturesChanged;
	FOnMosesPlayerZombieKillsChangedNative OnPlayerZombieKillsChanged;

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

	// ✅ [FIX][MOD]
	void BroadcastPlayerCaptures();
	void BroadcastPlayerZombieKills();

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

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCaptureComponent> CaptureComponent = nullptr;

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

	// [MOD] Capture count (SSOT)
	UPROPERTY(ReplicatedUsing = OnRep_Captures)
	int32 Captures = 0;

	// [MOD] Zombie kills (SSOT)
	UPROPERTY(ReplicatedUsing = OnRep_ZombieKills)
	int32 ZombieKills = 0;

	// PawnData는 외부가 GetPawnData<T>()로 조회
	UPROPERTY()
	TObjectPtr<UObject> PawnData = nullptr;
};
