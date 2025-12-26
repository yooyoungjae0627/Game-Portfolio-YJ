#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MosesPlayerState.generated.h"

class UMosesPawnData;
class UMosesExperienceDefinition;

/**
 * PlayerState
 * - 플레이어별 "유지되는 데이터" 저장소 (Seamless Travel에서도 유지 대상)
 * - Experience 로딩 완료 시점에 PawnData를 확정/저장한다.
 *
 * Day4 핵심:
 * - PersistentId/SelectedCharacterId/bReady 등을 넣고
 * - 로비->매치->로비 왕복에서도 값이 유지됨을 로그로 증명한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMosesPlayerState();

	/** ExperienceManager의 로딩 완료 이벤트를 등록하는 타이밍 */
	virtual void PostInitializeComponents() final;

	/** PlayerState가 보관 중인 PawnData 조회 */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** 디버깅 한 줄 로그용 (기존 유지) */
	FString MakeDebugString() const;

	/** ✅ DoD 고정 포맷 1줄 로그 (grep 판정용) */
	void DOD_PS_Log(const UObject* WorldContext, const TCHAR* Where) const;

	// ----- Lobby에서 설정되는 "유지 값" -----

	/** 플레이어 고정 ID(서버에서 생성). Travel 전/후 동일성 증명 핵심 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Persist")
	FGuid PersistentId;

	/** 로그 가독성용 이름 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Persist")
	FString DebugName;

	/** 로비에서 선택한 캐릭터(또는 클래스/프리셋 ID) */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Persist")
	int32 SelectedCharacterId = 0;

	/** 로비 Ready */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby")
	bool bReady = false;

	// 서버 전용 세터(서버 권한으로만 변경)
	void ServerSetSelectedCharacterId(int32 InId);
	void ServerSetReady(bool bInReady);

	// Seamless Travel 재생성 대비(상황에 따라 호출될 수 있음)
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Experience 로딩 완료 콜백 → PawnData 확정/저장 */
	void OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience);

	/** PawnData 최초 1회 세팅(중복 세팅 방지) */
	void SetPawnData(const UMosesPawnData* InPawnData);

	/** ✅ 서버에서만 PersistentId 부여(안전) */
	void EnsurePersistentId_ServerOnly();

private:
	/** 플레이어별 PawnData(Experience 기본값 또는 커스텀 값) */
	UPROPERTY()
	TObjectPtr<const UMosesPawnData> PawnData;
};
