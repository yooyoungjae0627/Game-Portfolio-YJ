// ============================================================================
// MosesPlayerState.h
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "MosesPlayerState.generated.h"

class UAbilitySystemComponent;
class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesPawnData;
class UMosesLobbyLocalPlayerSubsystem; // [ADD]

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedNative, int32 /*NewId*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMosesSelectedCharacterChangedBP, int32, NewId);

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

public:
	void TryInitASC(AActor* InAvatarActor);

public:
	FOnMosesSelectedCharacterChangedNative OnSelectedCharacterChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "Moses|Lobby")
	FOnMosesSelectedCharacterChangedBP OnSelectedCharacterChangedBP;

public:
	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }

	const FGuid& GetPersistentId() const { return PersistentId; }
	const FGuid& GetRoomId() const { return RoomId; }
	bool IsRoomHost() const { return bIsRoomHost; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }
	const FString& GetPlayerNickName() const { return PlayerNickName; }

	UMosesCombatComponent* GetCombatComponent() const { return CombatComponent; }
	const UMosesAttributeSet* GetMosesAttributeSet() const { return AttributeSet; }

public:
	void EnsurePersistentId_Server();
	void ServerSetLoggedIn(bool bInLoggedIn);
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacterId(int32 InId);

	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);
	void ServerSetPlayerNickName(const FString& InNickName);

public:
	void SetLoggedIn_Server(bool bInLoggedIn) { ServerSetLoggedIn(bInLoggedIn); } // 호환

	template <typename TPawnData>
	const TPawnData* GetPawnData() const
	{
		return Cast<TPawnData>(PawnData);
	}

	void SetPawnData(const UMosesPawnData* InPawnData) { PawnData = InPawnData; }

public:
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

private:
	UFUNCTION() void OnRep_PersistentId();
	UFUNCTION() void OnRep_LoggedIn();
	UFUNCTION() void OnRep_Ready();
	UFUNCTION() void OnRep_SelectedCharacterId();
	UFUNCTION() void OnRep_Room();
	UFUNCTION() void OnRep_PlayerNickName();

private:
	void BroadcastSelectedCharacterChanged(const TCHAR* Reason);
	void BindCombatDelegatesOnce();

	UFUNCTION()
	void HandleCombatDataChanged_BP(FString Reason);

	void HandleCombatDataChanged_Native(const TCHAR* Reason);

	// [ADD] 로컬 UI 갱신 파이프를 확실히 태우는 헬퍼
	void NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const;

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

	UPROPERTY(Transient)
	TObjectPtr<const UMosesPawnData> PawnData = nullptr;

private:
	UPROPERTY(VisibleAnywhere, Category = "Moses|Combat")
	TObjectPtr<UMosesCombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|GAS")
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesAttributeSet> AttributeSet = nullptr;

	UPROPERTY(Transient)
	bool bASCInitialized = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedAvatar;

	UPROPERTY(Transient)
	bool bCombatDelegatesBound = false;
};
