#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"     // IAbilitySystemInterface(ASC 접근 표준 인터페이스)
#include "MosesPlayerState.generated.h" 

class UAbilitySystemComponent;          // GetAbilitySystemComponent 반환 타입(전방선언)
class UMosesAbilitySystemComponent;     // 커스텀 ASC
class UMosesAttributeSet;               // 커스텀 AttributeSet

/**
 * AMosesPlayerState
 *
 * ✅ "단일 진실" 저장소(서버 authoritative)
 * - PlayerState는 Pawn보다 수명이 길다.
 * - Respawn(Avatar 교체), LateJoin, SeamlessTravel에서도 "개인 데이터"를 유지/복사할 수 있다.
 *
 * ✅ GAS 정책(Lyra 핵심)
 * - ASC OwnerActor   = PlayerState   (단일 진실, 영속)
 * - ASC AvatarActor  = Pawn/Character(조종되는 몸, 교체 가능)
 *
 * - InitAbilityActorInfo 중복 호출 방지 (같은 Avatar로 2번 호출되면 Ability/Tag/Effect 중복 꼬임)
 * - 단, Respawn 등 Avatar가 바뀌면 재초기화는 허용해야 함
 *
 * ✅ 네트워크 정책
 * - 서버가 값을 확정하고 Replication으로 클라에 배포
 * - 클라는 OnRep 시점에 UI 갱신(이벤트 기반), Tick polling 금지
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMosesPlayerState();

	// ---------------------------
	// Engine
	// ---------------------------

	/** 컴포넌트 생성 이후 호출되는 초기화 타이밍 */
	virtual void PostInitializeComponents() override;

	/** Replicated 필드 등록 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** GAS가 "이 액터의 ASC가 뭐냐" 물어볼 때 쓰는 표준 인터페이스 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** SeamlessTravel: Old PS -> New PS 로 복사(서버) */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	/** SeamlessTravel: New PS 가 Old PS 값으로 덮어쓰기(서버/클라) */
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	/** Attribute 접근(읽기용) */
	const UMosesAttributeSet* GetMosesAttributeSet() const { return AttributeSet; }

	// ---------------------------
	// GAS Init 
	// ---------------------------

	/**
	 * PS가 Pawn(Avatar)을 받아 ASC 초기화를 시도한다. (서버/클라 공용)
	 * - 서버: PossessedBy에서 호출
	 * - 클라: OnRep_PlayerState에서 호출
	 *
	 * 정책:
	 * - 같은 Avatar로 중복 Init 금지
	 * - Avatar가 바뀌면(Respawn/Reposssess) 재Init 허용
	 */
	void TryInitASC(AActor* InAvatarActor);

	// ---------------------------
	// Server-only Tag Policy
	// ---------------------------

	/** 서버에서만 Combat 태그를 On/Off 한다(클라 호출 금지) */
	void ServerSetCombatPhase(bool bEnable);

	/** 서버에서만 Dead 태그를 On/Off 한다(클라 호출 금지) */
	void ServerSetDead(bool bEnable);

	/** 디버그: 현재 초기화 완료 여부 */
	bool IsASCInitialized() const { return bASCInitialized; }

	// ---------------------------
	// Server authoritative setters
	// (주의: 서버에서만 호출하는 용도)
	// ---------------------------

	void ServerSetLoggedIn(bool bInLoggedIn);
	void ServerSetReady(bool bInReady);
	void ServerSetSelectedCharacterId(int32 InId);

	/** 룸 소속/호스트 상태 변경(서버 단일진실) */
	void ServerSetRoom(const FGuid& InRoomId, bool bInIsHost);

	// ---------------------------
	// Accessors (클라/UI는 무조건 이걸로)
	// ---------------------------

	bool IsLoggedIn() const { return bLoggedIn; }
	bool IsReady() const { return bReady; }

	const FGuid& GetPersistentId() const { return PersistentId; }
	const FGuid& GetRoomId() const { return RoomId; }
	bool IsRoomHost() const { return bIsRoomHost; }

	int32 GetSelectedCharacterId() const { return SelectedCharacterId; }
	FString GetPlayerNickName() const { return PlayerNickName; }

	// ---------------------------
	// PawnData access (Lyra-style 호출 호환)
	// ---------------------------

	template<typename T>
	const T* GetPawnData() const
	{
		// 개발자 주석:
		// - 외부 코드가 GetPawnData<...>()를 이미 호출 중이면
		//   PawnData 시스템 완성 전까지 nullptr 반환으로 연결만 맞춘다.
		return Cast<T>(PawnData);
	}

	// ---------------------------
	// Debug / DoD Logs
	// ---------------------------

	/** PS 핵심 데이터가 어떤 상태인지 로그 한 줄로 보여주기 */
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

	// ---------------------------
	// Server-only
	// ---------------------------

	/** 서버에서 PersistentId가 없으면 생성해서 세팅(보통 GameMode PostLogin에서 호출) */
	void EnsurePersistentId_Server();

	/** 서버 전용: 자동 로그인 처리(서버 단일진실) */
	void SetLoggedIn_Server(bool bInLoggedIn);

	/** 서버에서 닉네임 설정 */
	void ServerSetPlayerNickName(const FString& InNickName);

	/** 닉네임 RepNotify(클라에서 UI 갱신 트리거) */
	UFUNCTION()
	void OnRep_PlayerNickName();

private:
	// ---------------------------
	// Internal logs
	// ---------------------------

	/** ASC Init 로그 포맷을 통일하기 위한 헬퍼 */
	void LogASCInit(AActor* InAvatarActor) const;

	/** Attribute(HP/MaxHP) 로그 */
	void LogAttributes() const;

private:
	// ---------------------------
	// RepNotify (UI 동기화/로그 포인트)
	// ---------------------------

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

private:
	// ---------------------------
	// Replicated Fields (서버 단일진실)
	// ---------------------------

	/** 서버 발급 고유ID(룸/로그 시스템 PK) */
	UPROPERTY(ReplicatedUsing = OnRep_PersistentId)
	FGuid PersistentId;

	/** 로그인 완료 여부(서버 단일진실) */
	UPROPERTY(ReplicatedUsing = OnRep_LoggedIn)
	bool bLoggedIn = false;

	/** Ready 여부(서버 단일진실) */
	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;

	/** 선택 캐릭터 ID(서버 단일진실) */
	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId)
	int32 SelectedCharacterId = 1;

	/** 현재 소속 룸 ID(없으면 Invalid) */
	UPROPERTY(ReplicatedUsing = OnRep_Room)
	FGuid RoomId;

	/** 현재 룸의 호스트인지 */
	UPROPERTY(ReplicatedUsing = OnRep_Room)
	bool bIsRoomHost = false;

private:
	// ---------------------------
	// Debug display name (UI/로그 용)
	// ---------------------------

	/** 표시용 닉네임(서버에서 세팅, 클라에서 OnRep로 UI 반영) */
	UPROPERTY(ReplicatedUsing = OnRep_PlayerNickName)
	FString PlayerNickName;

	/** Lyra-style PawnData(완성 전엔 nullptr) */
	UPROPERTY()
	TObjectPtr<const UObject> PawnData = nullptr;

private:
	// ---------------------------
	// GAS Core (Lyra 핵심 구조)
	// ---------------------------

	/**
	 * ✅ ASC(Replicated)
	 * - OwnerActor = PlayerState
	 * - AvatarActor = Pawn(교체 가능)
	 */
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMosesAbilitySystemComponent> MosesAbilitySystemComponent;

	/** AttributeSet (ASC를 통해 Rep) */
	UPROPERTY()
	TObjectPtr<UMosesAttributeSet> AttributeSet;

	/**
	 * ✅ Init 1회 보장 가드
	 * - 같은 Avatar로 InitAbilityActorInfo가 2번 호출되면 꼬이므로 방지
	 */
	UPROPERTY(Transient)
	bool bASCInitialized = false;

	/**
	 * ✅ Avatar 교체 감지
	 * - Respawn/Reposssess로 Pawn이 바뀌면 재Init은 허용해야 함
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedAvatar;
};
