// PlayerCharacter.h
// ============================================================================
//  Player Pawn 역할 정리 (Lyra 스타일 / Server Authority / SSOT)
//
// [역할(중요)]
// - Pawn(APlayerCharacter)은 "입력 엔드포인트"이자 "코스메틱 표현" 담당.
// - 전투/장착/탄약/발사 승인(정답)은 서버가 내리며,
//   SSOT(Single Source of Truth)는 PlayerState 소유 CombatComponent가 가진다.
// - Pawn은 다음만 담당한다:
//   1) HeroComponent가 전달한 입력을 서버 요청 API로 연결
//   2) RepNotify/Delegate로 내려온 '상태'를 이용해 코스메틱(무기 메시/몽타주)을 재현
//
// [핵심 원칙]
// - 클라이언트는 "요청"만 한다. (Server RPC / Component Request)
// - 서버는 "승인 + 상태 변경"만 한다. (SSOT 변경)
// - 모든 클라는 "상태 복제 결과"로 동일하게 보이게 한다. (OnRep -> Delegate -> Cosmetic)
// - HUD/UMG는 Tick 금지(이 클래스는 코스메틱만).
//
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "PlayerCharacter.generated.h"

class AController;
class APickupBase;

class UMosesHeroComponent;
class UMosesCameraComponent;
class UMosesCombatComponent;

class USkeletalMeshComponent;
class UAnimMontage;

UCLASS()
class UE5_MULTI_SHOOTER_API APlayerCharacter : public AMosesCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// =========================================================================
	// Query
	// =========================================================================
	// 현재 스프린트 상태(서버 확정값)
	bool IsSprinting() const;

	// =========================================================================
	// Input Endpoints (HeroComponent -> Pawn)
	// - EnhancedInput 바인딩은 HeroComponent에서 수행하고
	//   이 함수들을 호출해 Pawn 로직을 트리거한다.
	// =========================================================================
	void Input_Move(const FVector2D& MoveValue);
	void Input_Look(const FVector2D& LookValue);

	void Input_SprintPressed();
	void Input_SprintReleased();

	void Input_JumpPressed();
	void Input_InteractPressed();

	void Input_EquipSlot1();
	void Input_EquipSlot2();
	void Input_EquipSlot3();

	// Fire 입력(요청만)
	void Input_FirePressed();

protected:
	// =========================================================================
	// Actor lifecycle / Possession hooks
	// - PlayerState / Controller가 붙는 타이밍이 케이스마다 달라서
	//   아래 훅들에서 CombatComponent 바인딩을 보강한다.
	// =========================================================================
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnRep_Controller() override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

public:
	// =========================================================================
	// Montage Cosmetics (Server approved -> Multicast)
	// - 서버가 "발사/피격/사망"을 승인한 뒤에만 호출되어야 한다.
	// - 실제 호출 주체는 CombatComponent(Server 로직)다.
	// =========================================================================

	// 발사/피격은 신뢰성 낮아도 OK(자주 발생, 유실되어도 치명적이지 않음)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireMontage();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReactMontage();

	// 사망은 유실되면 안 되므로 Reliable
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage();

private:
	// =========================================================================
	// Sprint (Client feel + Server authority)
	// - 체감: 로컬은 즉시 속도 반영(예측)
	// - 정답: 서버가 bIsSprinting을 확정하고 Replication으로 내려준다
	// =========================================================================
	void ApplySprintSpeed_LocalPredict(bool bNewSprinting);
	void ApplySprintSpeed_FromAuth(const TCHAR* From);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

private:
	// =========================================================================
	// Pickup 
	// - 로컬에서 타겟 탐색 -> 서버에 TryPickup 요청
	// - 현재는 placeholder 
	// =========================================================================
	APickupBase* FindPickupTarget_Local() const;

	UFUNCTION(Server, Reliable)
	void Server_TryPickup(APickupBase* Target);

private:
	// =========================================================================
	// CombatComponent Bind (RepNotify -> Delegate -> Cosmetic)
	// - PlayerState(SSOT) 소유 CombatComponent의 델리게이트를 구독하여
	//   장착 상태 변경이 오면 무기 코스메틱을 갱신한다.
	// =========================================================================
	void BindCombatComponent();
	void UnbindCombatComponent();

	void HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId);
	void RefreshWeaponCosmetic(FGameplayTag WeaponId);

	// 코스메틱 무기 메시 컴포넌트 생성/등록/소켓 부착 보장
	void EnsureWeaponMeshComponent();

	// PlayerState(SSOT)에서 CombatComponent 획득
	UMosesCombatComponent* GetCombatComponent_Checked() const;

	// 몽타주 로컬 재생 헬퍼
	void TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const;

private:
	// =========================================================================
	// Components
	// =========================================================================

	// (정책) 입력은 HeroComponent가 처리하고 Pawn의 Input_*을 호출한다.
	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent = nullptr;

	// 카메라 모드 스택(프로젝트 기준 MosesCameraComponent)
	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> MosesCameraComponent = nullptr;

	// 코스메틱 무기 표시용 (Actor 스폰 X / Replicate X)
	// - 장착 변경 시 SkeletalMesh만 교체하여 손 소켓에 붙여 보이게 한다.
	UPROPERTY(VisibleAnywhere, Category = "Moses|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp = nullptr;

private:
	// =========================================================================
	// Tunables
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Move")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Pickup")
	float PickupTraceDistance = 500.0f;

	// 캐릭터 스켈레톤에 존재해야 하는 소켓 이름
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FName CharacterWeaponSocketName = TEXT("WeaponSocket");

private:
	// =========================================================================
	// Montages (BP에서 세팅)
	// - 실제 재생되려면 ABP에 Slot/LayeredBlend 구성이 되어 있어야 한다.
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

private:
	// =========================================================================
	// Replicated State
	// =========================================================================

	// 서버 확정 스프린트 상태(정답)
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	// 로컬 체감용 예측 플래그(네트워크 정답 아님)
	UPROPERTY(Transient)
	bool bLocalPredictedSprinting = false;

	// CombatComponent 캐시(델리게이트 바인딩 관리)
	UPROPERTY(Transient)
	TObjectPtr<UMosesCombatComponent> CachedCombatComponent = nullptr;
};
