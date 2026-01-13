#pragma once

#include "GameFramework/PlayerState.h"
#include "MosesPlayerState.generated.h"

/**
 * AMosesPlayerState
 *
 * 단일진실(플레이어 개인 상태)
 * - Travel(SeamlessTravel)에서도 CopyProperties/OverrideWith로 유지되는 "개인 데이터" 저장소
 *
 * 핵심:
 * - PersistentId : 서버 발급 고유ID(룸/로그 시스템의 Primary Key)
 * - RoomId       : 현재 소속 룸(서버 authoritative)
 * - bIsRoomHost  : 현재 룸의 호스트 여부(서버 authoritative)
 * - bReady       : 로비 Ready 여부(서버 authoritative)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMosesPlayerState();

	// ---------------------------
	// Engine
	// ---------------------------
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SeamlessTravel: Old PS -> New PS 로 복사 (서버) */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	/** SeamlessTravel: New PS 가 Old PS 값으로 덮어쓰기 (클라/서버) */
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	// ---------------------------
	// Server authoritative setters
	// (주의: 이 함수들은 "서버에서만" 호출하는 용도)
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
		// - 네 프로젝트 다른 코드(MosesGameModeBase)가 GetPawnData<UMosesPawnData>()를 이미 호출한다.
		// - 아직 PawnData 시스템이 완성되지 않았으면 nullptr 반환으로 연결만 맞춘다.
		return Cast<T>(PawnData);
	}

	// ---------------------------
	// Debug / DoD Logs
	// ---------------------------
	void DOD_PS_Log(const UObject* Caller, const TCHAR* Phase) const;

	// ---------------------------
	// Server-only
	// ---------------------------
	/** 서버에서 PersistentId가 없으면 생성해서 세팅한다. (PostLogin에서 호출) */
	void EnsurePersistentId_Server();


	/** 서버 전용: 자동 로그인 처리 */
	void SetLoggedIn_Server(bool bInLoggedIn);

	void ServerSetPlayerNickName(const FString& InNickName);

	UFUNCTION()
	void OnRep_PlayerNickName();

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

	UPROPERTY(ReplicatedUsing = OnRep_PersistentId)
	FGuid PersistentId;

	UPROPERTY(ReplicatedUsing = OnRep_LoggedIn)
	bool bLoggedIn = false;

	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;

	// 개발자 주석:
	// - 캐릭터는 2개 고정.
	// - 디폴트는 1번 캐릭터(=TypeA).
	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId)
	int32 SelectedCharacterId = 1;

	/** 현재 소속 룸 ID (없으면 Invalid) */
	UPROPERTY(ReplicatedUsing = OnRep_Room)
	FGuid RoomId;

	/** 현재 룸의 호스트인지 */
	UPROPERTY(ReplicatedUsing = OnRep_Room)
	bool bIsRoomHost = false;

private:
	// ---------------------------
	// Debug display name (UI/로그 용)
	// ---------------------------
	UPROPERTY(ReplicatedUsing = OnRep_PlayerNickName)
	FString PlayerNickName;

	UPROPERTY()
	TObjectPtr<const UObject> PawnData = nullptr;
};
