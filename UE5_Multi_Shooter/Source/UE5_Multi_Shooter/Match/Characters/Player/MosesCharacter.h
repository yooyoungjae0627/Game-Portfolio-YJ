// ============================================================================
// UE5_Multi_Shooter/Match/Characters/Player/MosesCharacter.h
// ----------------------------------------------------------------------------
// AMosesCharacter
//
// 역할:
// - Player / Enemy 공통 "얇은 Body Pawn"
// - 입력 / 카메라 / 전투 SSOT(HP, Ammo 등) 로직을 절대 두지 않는다.
//
// GAS 정책:
// - ASC Owner  = PlayerState
// - ASC Avatar = Pawn(=Character)
// - 따라서 PossessedBy / OnRep_PlayerState / BeginPlay 에서
//   PlayerState::TryInitASC(this) 보강 호출이 반드시 필요
//
// 설계 원칙:
// - GameMode = 결정
// - PlayerState = SSOT
// - Pawn = 표현(Avatar)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MosesCharacter.generated.h"

class UMosesPawnSSOTGuardComponent;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMosesCharacter();

protected:
	// =========================================================================
	// Engine Lifecycle
	// =========================================================================
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:
	// =========================================================================
	// Server Authority Hooks (Override 가능)
	// - CombatComponent가 서버 판정 후 Pawn에 통지하는 훅
	// =========================================================================

	/** 서버에서 Damage 확정 후 호출되는 공통 훅 */
	virtual void HandleDamaged(float DamageAmount, AActor* DamageCauser);

	/** 서버에서 Death 확정 후 호출되는 공통 훅 */
	virtual void HandleDeath(AActor* DeathCauser);

public:
	// =========================================================================
	// Server Pipeline Wrappers
	// - 외부(CombatComponent)에서 Pawn으로 서버 훅을 호출할 수 있게 한다.
	// - Authority Guard 포함
	// =========================================================================

	void ServerNotifyDamaged(float DamageAmount, AActor* DamageCauser);
	void ServerNotifyDeath(AActor* DeathCauser);

public:
	// =========================================================================
	// Cosmetic Triggers (All Clients)
	// - 최소 구현: 로그만
	// - 실제 플레이어는 APlayerCharacter에서 몽타주 override
	// =========================================================================

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReactCosmetic();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathCosmetic();

private:
	// =========================================================================
	// GAS Init Helper
	// - Pawn → PlayerState ASC 초기화 보강
	// - Possessed / OnRep / BeginPlay에서 호출
	// =========================================================================
	void TryInitASC_FromPawn(const TCHAR* Reason);

private:
	// =========================================================================
	// Movement Config
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float DefaultWalkSpeed = 600.0f;

private:
	// =========================================================================
	// SSOT Guard
	// - Pawn에 HP/Ammo 같은 전투 상태를 두지 말라는 정책 감지기
	// =========================================================================
	UPROPERTY(VisibleAnywhere, Category = "Moses|Guard")
	TObjectPtr<UMosesPawnSSOTGuardComponent> PawnSSOTGuard;
};
