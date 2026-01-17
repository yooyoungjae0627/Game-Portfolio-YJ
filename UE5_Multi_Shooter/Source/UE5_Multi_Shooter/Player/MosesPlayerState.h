#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "MosesPlayerState.generated.h"

// Forward Declarations
class UAbilitySystemComponent;
class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;

/**
 * AMosesPlayerState
 *
 * [SSOT: Single Source of Truth]
 * - PlayerState는 Pawn보다 수명이 길어, Respawn/SeamlessTravel/LateJoin에서도 유지된다.
 *
 * [GAS: Lyra Style]
 * - ASC OwnerActor  = PlayerState
 * - ASC AvatarActor = Pawn/Character
 *
 * [Network Policy]
 * - 서버가 확정하고(Authority), 클라는 RepNotify(OnRep)로만 UI를 갱신한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState();

	// ------------------------------------------------------------
	// Engine / Net
	// ------------------------------------------------------------
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** IAbilitySystemInterface */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** SeamlessTravel: OldPS -> NewPS (Server) */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	/** SeamlessTravel: NewPS overwrites from OldPS (Server/Client) */
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	// ------------------------------------------------------------
	// GAS Init
	// ------------------------------------------------------------
	void TryInitASC(AActor* InAvatarActor);

	// ------------------------------------------------------------
	// Server-only GameplayTag Policy
	// ------------------------------------------------------------
	void ServerSetCombatPhase(bool bEnable);
	void ServerSetDead(bool bEnable);

	// ------------------------------------------------------------
	// Server authoritative setters (Server SSOT)
	// ------------------------------------------------------------
	void ServerSetLoggedIn(bool bInLoggedIn);
	void ServerSetReady(bool bInReady);

	/** 로비에서 선택한 캐릭터 Index 확정 (Server RPC) */
	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacterId(int32 InId);

	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);

	// ------------------------------------------------------------
	// Accessors (Client/UI must use getters)
	// ------------------------------------------------------------
	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }

	const FGuid& GetPersistentId() const { return PersistentId; }
	const FGuid& GetRoomId() const { return RoomId; }
	bool IsRoomHost() const { return bIsRoomHost; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }
	const FString& GetPlayerNickName() const { return PlayerNickName; }

	UMosesCombatComponent* GetCombatComponent() const { return CombatComponent; }
	const UMosesAttributeSet* GetMosesAttributeSet() const { return AttributeSet; }

	// ------------------------------------------------------------
	// PawnData access (Lyra-style compatibility)
	// ------------------------------------------------------------
	template<typename T>
	const T* GetPawnData() const // [MOD-FIX] 프로젝트에서 이미 호출 중이므로 반드시 제공
	{
		return Cast<T>(PawnData);
	}

	// ------------------------------------------------------------
	// Debug / DoD Logs
	// ------------------------------------------------------------
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

	// ------------------------------------------------------------
	// Server-only helpers
	// ------------------------------------------------------------
	void EnsurePersistentId_Server();
	void SetLoggedIn_Server(bool bInLoggedIn);
	void ServerSetPlayerNickName(const FString& InNickName);

	UFUNCTION()
	void OnRep_PlayerNickName();

private:
	// Internal logs
	void LogASCInit(AActor* InAvatarActor) const;
	void LogAttributes() const;

	// RepNotify
	UFUNCTION() void OnRep_PersistentId();
	UFUNCTION() void OnRep_LoggedIn();
	UFUNCTION() void OnRep_Ready();
	UFUNCTION() void OnRep_SelectedCharacterId();
	UFUNCTION() void OnRep_Room();

private:
	// ------------------------------------------------------------
	// Replicated Fields (SSOT)
	// ------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_PersistentId)
	FGuid PersistentId;

	UPROPERTY(ReplicatedUsing = OnRep_LoggedIn)
	bool bLoggedIn = false;

	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;

	/** LobbyGM 서버가 확정한 Catalog index */
	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId)
	int32 SelectedCharacterId = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Room)
	FGuid RoomId;

	UPROPERTY(ReplicatedUsing = OnRep_Room)
	bool bIsRoomHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerNickName)
	FString PlayerNickName;

private:
	// Components / Data
	UPROPERTY(VisibleAnywhere, Category = "Moses|Combat")
	TObjectPtr<UMosesCombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|GAS")
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UMosesAttributeSet> AttributeSet = nullptr;

	/** Lyra-style PawnData placeholder (완성 전까지 nullptr 허용) */
	UPROPERTY()
	TObjectPtr<const UObject> PawnData = nullptr; // [MOD-FIX] GetPawnData<T>() 지원을 위해 유지

	// GAS Init Guards
	UPROPERTY(Transient)
	bool bASCInitialized = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedAvatar;
};
