#pragma once 

#include "MosesGameModeBase.h" 
#include "MosesLobbyGameMode.generated.h" 

class AMosesPlayerController;

/**
 * Lobby GameMode
 *
 * 책임:
 * - 로비 서버 전용 규칙 관리
 * - Start 요청 검증 (Host / Ready / 인원 조건)
 * - Match 맵으로 ServerTravel 트리거
 *
 * 주의:
 * - GameMode는 서버 전용 객체
 * - 클라이언트 로직/UI 절대 두지 말 것
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameMode();

	/**
	 * Start 버튼 요청 처리 (서버 전용)
	 * - 요청자 권한 검증
	 * - 룸 상태 검증
	 * - 조건 통과 시 Match로 ServerTravel
	 */
	void HandleStartGameRequest(AMosesPlayerController* RequestPC);

	/** 로비 맵 로드 시 옵션 정리 (Experience 강제) */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** 로비 월드 진입 완료 시점 */
	virtual void BeginPlay() override;

	/**
	 * 플레이어 로그인 완료 직후 호출
	 * - SeamlessTravel 복귀 시에도 호출됨
	 * - 로비 복귀 정책 적용 지점
	 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/**
	 * 플레이어 연결 종료 처리
	 * - Disconnect / 강종 포함
	 * - 룸 상태 정리 최종 지점
	 */
	virtual void Logout(AController* Exiting) override;

	/**
	 * SeamlessTravel 중 PlayerController / PlayerState 인계 시점
	 * - PS 유지 여부 검증 로그 목적
	 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	/**
	 * SeamlessTravel 시 유지할 Actor 목록 결정
	 * - 기본 구현 + 필요 시 확장
	 */
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

	/** 서버 콘솔 수동 매치 이동 (디버그용) */
	UFUNCTION(Exec)
	void TravelToMatch();

protected:
	/**
	 * Experience READY 이후 1회 호출
	 * - Lobby 시스템 초기화 확정 지점
	 * - ServerPhase = Lobby 고정
	 */
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience) override;

private:
	/** Match 맵 URL 정책 단일화 */
	FString GetMatchMapURL() const;

	/** PlayerState 포인터/ID/이름 덤프 (디버그용) */
	void DumpPlayerStates(const TCHAR* Prefix) const;

	/** DOD 검증용 PlayerState 고정 포맷 로그 */
	void DumpAllDODPlayerStates(const TCHAR* Where) const;

	/** ServerTravel 호출 가능 여부 검증 */
	bool CanDoServerTravel() const;

	/**
	 * 로비 복귀 정책 적용
	 * - Ready 상태 초기화
	 */
	void ApplyReturnPolicy(APlayerController* PC);

	int32 GetPlayerCount() const;
	int32 GetReadyCount() const;
};
