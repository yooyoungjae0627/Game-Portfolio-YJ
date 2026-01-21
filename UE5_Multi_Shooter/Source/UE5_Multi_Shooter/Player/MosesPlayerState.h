#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h" 
#include "MosesPlayerState.generated.h"

class UAbilitySystemComponent;
class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesPawnData;
class UMosesLobbyLocalPlayerSubsystem;

// -------------------------
// Lobby delegates (existing)
// -------------------------
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedNative, int32 /*NewId*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedBP, int32, NewId);

// -------------------------
// HUD delegates 
// -------------------------
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesVitalChangedNative, float /*Current*/, float /*Max*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesAmmoChangedNative, int32 /*Mag*/, int32 /*Reserve*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesIntChangedNative, int32 /*Value*/);

/**
 * AMosesPlayerState
 *
 * [SSOT 정책]
 * - 전투 상태/속성/점수 등 "진짜 값"은 PlayerState가 단일 진실로 소유한다.
 *
 * [GAS 정책]
 * - HP/Shield는 무조건 GAS(AttributeSet)로 관리한다.
 * - ASC Owner = PlayerState, Avatar = Pawn (AMosesCharacter가 TryInitASC 호출)
 *
 * [HUD 정책]
 * - Tick/Binding 금지
 * - RepNotify / GAS AttributeChange -> Delegate -> HUD 갱신
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ----------------------------
	// AActor / APlayerState
	// ----------------------------
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	// Score는 기본 APlayerState::Score를 쓰고, Rep 시점에 브로드캐스트만 한다.  // [ADD]
	virtual void OnRep_Score() override;

	// ----------------------------
	// IAbilitySystemInterface
	// ----------------------------
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ----------------------------
	// GAS init (called from Pawn)
	// ----------------------------
	void TryInitASC(AActor* InAvatarActor);

	// ----------------------------
	// Getters
	// ----------------------------
	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }

	const FGuid& GetPersistentId() const { return PersistentId; }
	const FGuid& GetRoomId() const { return RoomId; }
	bool IsRoomHost() const { return bIsRoomHost; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }
	const FString& GetPlayerNickName() const { return PlayerNickName; }

	UMosesCombatComponent* GetCombatComponent() const { return CombatComponent; }
	const UMosesAttributeSet* GetMosesAttributeSet() const { return AttributeSet; }

	// ----------------------------
	// Lobby delegates (existing)
	// ----------------------------
	FOnMosesSelectedCharacterChangedNative OnSelectedCharacterChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "Moses|Lobby")
	FOnMosesSelectedCharacterChangedBP OnSelectedCharacterChangedBP;

	// ----------------------------
	// HUD delegates 
	// ----------------------------
	FOnMosesVitalChangedNative OnHealthChanged;
	FOnMosesVitalChangedNative OnShieldChanged;

	FOnMosesIntChangedNative OnScoreChanged;
	FOnMosesIntChangedNative OnDeathsChanged;

	FOnMosesAmmoChangedNative OnAmmoChanged;
	FOnMosesIntChangedNative OnGrenadeChanged;

	// ----------------------------
	// Server-side setters (existing)
	// ----------------------------
	void EnsurePersistentId_Server();
	void ServerSetLoggedIn(bool bInLoggedIn);
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacterId(int32 InId);

	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);
	void ServerSetPlayerNickName(const FString& InNickName);

	// ----------------------------
	// PawnData helpers
	// ----------------------------
	template <typename TPawnData>
	const TPawnData* GetPawnData() const
	{
		return Cast<TPawnData>(PawnData);
	}

	void SetPawnData(const UMosesPawnData* InPawnData) { PawnData = InPawnData; }

	// ----------------------------
	// Deaths(Defeats)  
	// ----------------------------
	void ServerAddDeath(); // 서버에서만 호출

	// ----------------------------
	// Debug
	// ----------------------------
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

private:
	// ----------------------------
	// RepNotifies (existing)
	// ----------------------------
	UFUNCTION() void OnRep_PersistentId();
	UFUNCTION() void OnRep_LoggedIn();
	UFUNCTION() void OnRep_Ready();
	UFUNCTION() void OnRep_SelectedCharacterId();
	UFUNCTION() void OnRep_Room();
	UFUNCTION() void OnRep_PlayerNickName();

	// Deaths RepNotify 
	UFUNCTION() void OnRep_Deaths();

	// ----------------------------
	// Lobby helpers (existing)
	// ----------------------------
	void BroadcastSelectedCharacterChanged(const TCHAR* Reason);
	void NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const;

	// ----------------------------
	// Combat delegates (existing + HUD bridge) 
	// ----------------------------
	void BindCombatDelegatesOnce();

	UFUNCTION()
	void HandleCombatDataChanged_BP(FString Reason);

	void HandleCombatDataChanged_Native(const TCHAR* Reason);

	// HUD bridge helpers  
	void BroadcastAmmoAndGrenade();
	void BroadcastScore();
	void BroadcastDeaths();

	// ----------------------------
	// GAS attribute delegates -> HUD delegate bridge 
	// ----------------------------
	void BindASCAttributeDelegates();
	void BroadcastVitals_Initial();

	void HandleHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleShieldChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged_Internal(const FOnAttributeChangeData& Data);

private:
	// ----------------------------
	// Replicated (existing)
	// ----------------------------
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

	// Defeats(Deaths)  
	UPROPERTY(ReplicatedUsing = OnRep_Deaths)
	int32 Deaths = 0;

private:
	// ----------------------------
	// Runtime (non-replicated)
	// ----------------------------
	UPROPERTY(Transient)
	TObjectPtr<const UMosesPawnData> PawnData = nullptr;

private:
	// ----------------------------
	// Components (SSOT)
	// ----------------------------
	UPROPERTY(VisibleAnywhere, Category = "Moses|Combat")
	TObjectPtr<UMosesCombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|GAS")
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesAttributeSet> AttributeSet = nullptr;

private:
	// ----------------------------
	// Guards
	// ----------------------------
	UPROPERTY(Transient)
	bool bASCInitialized = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedAvatar;

	UPROPERTY(Transient)
	bool bCombatDelegatesBound = false;

	UPROPERTY(Transient) 
	bool bASCDelegatesBound = false;
};
