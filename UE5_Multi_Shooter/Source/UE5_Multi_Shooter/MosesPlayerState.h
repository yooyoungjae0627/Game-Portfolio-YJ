// ============================================================================
// UE5_Multi_Shooter/MosesPlayerState.h (FULL)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"

#include "UE5_Multi_Shooter/Match/GAS/MosesAbilitySet.h"
#include "UE5_Multi_Shooter/Match/GAS/AttributeSet/MosesAttributeSet.h"

#include "MosesPlayerState.generated.h"

class AActor;
class UMosesAbilitySystemComponent;
class UMosesAttributeSet;
class UMosesCombatComponent;
class UMosesSlotOwnershipComponent;
class UMosesAbilitySet;
class UMosesCaptureComponent;
class UGameplayEffect;

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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMosesTotalScoreChangedNative, int32 /*TotalScore*/);

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

protected:
	/** PostInitializeComponents: 컴포넌트 초기화 이후 추가 셋업 지점. */
	virtual void PostInitializeComponents() override;

	/** Replication 목록 설정. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seamless Travel/재접속 등에서 PS 데이터 복사. */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	/** Seamless Travel/재접속 등에서 PS 데이터 덮어쓰기. */
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	/** Score(기본 APlayerState score) Rep 수신 시 브로드캐스트. */
	virtual void OnRep_Score() override;

public:
	/** PawnData 조회 헬퍼(템플릿). */
	template<typename TPawnData>
	const TPawnData* GetPawnData() const
	{
		return Cast<TPawnData>(PawnData);
	}

	/** AbilitySystemInterface */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	AMosesPlayerState(const FObjectInitializer& ObjectInitializer);

	/* Basic getters */

	const FGuid& GetPersistentId() const { return PersistentId; }
	const FString& GetPlayerNickName() const { return PlayerNickName; }

	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }
	bool IsRoomHost() const { return bIsRoomHost; }
	const FGuid& GetRoomId() const { return RoomId; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }

	UMosesSlotOwnershipComponent* GetSlotOwnershipComponent() const { return SlotOwnershipComponent; }
	UMosesCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/* Match stats getters */

	int32 GetDeaths() const { return Deaths; }
	int32 GetCaptures() const { return Captures; }
	int32 GetZombieKills() const { return ZombieKills; }
	int32 GetPvPKills() const { return PvPKills; }
	int32 GetHeadshots() const { return Headshots; }
	int32 GetTotalKills() const { return PvPKills + ZombieKills; }
	int32 GetTotalScore() const { return TotalScore; }

	/* Death state getters */

	bool IsDead() const { return bIsDead; }
	float GetRespawnEndServerTime() const { return RespawnEndServerTime; }

	/* GAS attribute getters */

	float GetHealth_Current() const;
	float GetHealth_Max() const;
	float GetShield_Current() const;
	float GetShield_Max() const;

	/* GAS init */

	/** ASC 초기화(Owner/Avatar 설정) + 기본 Attribute 세팅(서버) + 델리게이트 바인딩 + 브로드캐스트. */
	void TryInitASC(AActor* InAvatarActor);

	/* Death / Respawn */

	/** GAS(데미지/죽음)에서 서버 권위로 죽음 확정 처리. */
	void ServerNotifyDeathFromGAS();

	/** Respawn 완료 시 Dead 플래그/시간을 정리한다. */
	void ServerClearDeadAfterRespawn();

	/* Shield regen (Server) */

	/** 실드 리젠 타이머 시작(서버). */
	void ServerStartShieldRegen();

	/** 실드 리젠 타이머 종료(서버). */
	void ServerStopShieldRegen();

	/** 실드 리젠에 사용할 GameplayEffect(1 tick 단위). */
	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS")
	TSubclassOf<UGameplayEffect> GE_ShieldRegen_One;

	/* Weapon/AbilitySet (Server) */

	/** 매치 기본 로드아웃(라이플+탄) 지급을 1회 보장한다. */
	void ServerEnsureMatchDefaultLoadout();

	/** 전투 AbilitySet을 1회만 적용한다. */
	void ServerApplyCombatAbilitySetOnce(UMosesAbilitySet* InAbilitySet);

	/* Lobby / Info (Server) */

	/** PersistentId가 없으면 서버에서 생성한다. */
	void EnsurePersistentId_Server();

	/** 로그인 상태 설정(서버). */
	void ServerSetLoggedIn(bool bInLoggedIn);

	/** Ready 상태 설정(서버). */
	void ServerSetReady(bool bInReady);

	/** 캐릭터 선택 ID 설정(서버). */
	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacterId(int32 InId);

	/** 룸 정보 설정(서버). */
	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);

	/** 닉네임 설정(서버). */
	void ServerSetPlayerNickName(const FString& InNickName);

	/* Score / Stats (Server) */

	/** 기본 Score(APlayerState::Score)를 델타만큼 변경한다(서버). */
	void ServerAddScore(int32 Delta, const TCHAR* Reason);

	/** 데스 +1(서버). */
	void ServerAddDeath();

	/** 캡처 +Delta(서버). */
	void ServerAddCapture(int32 Delta = 1);

	/** 좀비킬 +Delta(서버). */
	void ServerAddZombieKill(int32 Delta = 1);

	/** PvP킬 +Delta(서버). */
	void ServerAddPvPKill(int32 Delta = 1);

	/** 헤드샷 +Delta(서버). */
	void ServerAddHeadshot(int32 Delta = 1);

	/** 결과 진입 순간 TotalScore를 1회 세팅(서버). */
	void ServerSetTotalScore(int32 NewTotalScore);

	/* RepNotifies */

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

	UFUNCTION()
	void OnRep_TotalScore();

	/* Delegates (Native) */

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
	FOnMosesTotalScoreChangedNative OnTotalScoreChanged;

	/* Delegates (BP) */

	UPROPERTY(BlueprintAssignable)
	FOnMosesSelectedCharacterChangedBP OnSelectedCharacterChangedBP;

	/* Debug */

	/** 상태 덤프 로그. */
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

	/* Damage GE policy */

	/** Player Damage SetByCaller GE 로드(SoftClassPtr). */
	TSubclassOf<UGameplayEffect> GetDamageGE_Player_SetByCaller() const;

	/** Zombie Damage SetByCaller GE 로드(SoftClassPtr). */
	TSubclassOf<UGameplayEffect> GetDamageGE_Zombie_SetByCaller() const;

private:
	/* Lobby bridge */

	/** Lobby UI 브릿지(LocalPlayerSubsystem)에게 PlayerState 변경을 알린다(로컬 컨트롤러만). */
	void NotifyLobbyPlayerStateChanged_Local(const TCHAR* Reason) const;

	/* GAS delegates */

	/** AttributeChange 델리게이트를 1회 바인딩한다. */
	void BindASCAttributeDelegates();

	/** 초기 Vitals 브로드캐스트(HP/Shield). */
	void BroadcastVitals_Initial();

	/* Broadcast helpers */

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
	void BroadcastTotalScore();

	/* Attribute change handlers */

	void HandleHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleShieldChanged_Internal(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged_Internal(const FOnAttributeChangeData& Data);

private:
	/* =========================================================================
	 * Variables (UPROPERTY first, then non-UPROPERTY)
	 * ========================================================================= */

	 /* Components */

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

	/* Cached pointers / init flags */

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedAvatar;

	bool bASCInitialized = false;
	bool bASCDelegatesBound = false;
	bool bCombatAbilitySetApplied = false;

	UPROPERTY()
	bool bMatchDefaultLoadoutGranted = false;

	FMosesAbilitySet_GrantedHandles CombatAbilitySetHandles;

	/* Lobby identity / room state */

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

	/* Match stats (replicated) */

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

	UPROPERTY(ReplicatedUsing = OnRep_TotalScore)
	int32 TotalScore = 0;

	/* Death state (replicated) */

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	float RespawnEndServerTime = 0.f;

	/* Pawn data */

	UPROPERTY()
	TObjectPtr<UObject> PawnData = nullptr;

	/* Damage GE policy */

	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS|Damage")
	TSoftClassPtr<UGameplayEffect> DamageGE_Player_SetByCaller;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|GAS|Damage")
	TSoftClassPtr<UGameplayEffect> DamageGE_Zombie_SetByCaller;

	/* Non-UPROPERTY */

	FTimerHandle TimerHandle_ShieldRegen;
};
