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
//   2) RepNotify/Delegate로 내려온 '상태'를 이용해 코스메틱(무기 메시/몽타주/SFX/VFX)을 재현
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
class USoundBase;
class UNiagaraSystem;

UCLASS()
class UE5_MULTI_SHOOTER_API APlayerCharacter : public AMosesCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// =========================================================================
	// Query
	// =========================================================================
	bool IsSprinting() const;

	// =========================================================================
	// Input Endpoints (HeroComponent -> Pawn)
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
	// =========================================================================
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * PostInitializeComponents
	 *
	 * - 생성자보다 늦고 BeginPlay보다 빠른 안정 구간.
	 * - 코스메틱 컴포넌트의 Attach 보강 같은 "에디터/PIE 흔들림 방어"에 적합하다.
	 */
	virtual void PostInitializeComponents() override;

	virtual void OnRep_Controller() override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

public:
	// =========================================================================
	// Montage Cosmetics (Server approved -> Multicast)
	// =========================================================================

	// 발사/피격은 자주 발생하므로 Unreliable 허용(유실되어도 치명적이지 않음)
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
	// =========================================================================
	APickupBase* FindPickupTarget_Local() const;

	UFUNCTION(Server, Reliable)
	void Server_TryPickup(APickupBase* Target);

private:
	// =========================================================================
	// CombatComponent Bind (RepNotify -> Delegate -> Cosmetic)
	// =========================================================================
	void BindCombatComponent();
	void UnbindCombatComponent();

	void HandleEquippedChanged(int32 SlotIndex, FGameplayTag WeaponId);

	/** [DAY3] SSOT 죽음 상태 변경 수신 */
	void HandleDeadChanged(bool bNewDead);

	void RefreshWeaponCosmetic(FGameplayTag WeaponId);

	// PlayerState(SSOT)에서 CombatComponent 획득
	UMosesCombatComponent* GetCombatComponent_Checked() const;

	// 몽타주 로컬 재생 헬퍼
	void TryPlayMontage_Local(UAnimMontage* Montage, const TCHAR* DebugTag) const;

	// [DAY3] Fire SFX/VFX 로컬 재생/스폰 (Multicast에서 호출)
	void PlayFireAV_Local() const;

	// [DAY3] Dead 시 이동/입력 코스메틱 차단(모든 머신에서 동일하게)
	void ApplyDeadCosmetics_Local() const;

private:
	// =========================================================================
	// Components
	// =========================================================================
	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesHeroComponent> HeroComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Moses|Components")
	TObjectPtr<UMosesCameraComponent> MosesCameraComponent = nullptr;

	// 코스메틱 무기 표시용 (Actor 스폰 X / Replicate X)
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

	// 캐릭터 스켈레톤에 존재해야 하는 소켓 이름(WeaponMeshComp가 여기로 붙음)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FName CharacterWeaponSocketName = TEXT("WeaponSocket");

	// 무기(WeaponMeshComp)에 존재해야 하는 총구 소켓 이름(SFX/VFX 스폰 기준)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon")
	FName WeaponMuzzleSocketName = TEXT("MuzzleSocket");

private:
	// =========================================================================
	// Montages / SFX / VFX (BP에서 세팅)
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Anim")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	// [DAY3] 발사 사운드(단일 검증용)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|SFX")
	TObjectPtr<USoundBase> FireSound = nullptr;

	// [DAY3] 총구 섬광(단일 검증용)
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Weapon|VFX")
	TObjectPtr<UNiagaraSystem> MuzzleFlashFX = nullptr;

private:
	// =========================================================================
	// Replicated State
	// =========================================================================
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(Transient)
	bool bLocalPredictedSprinting = false;

	UPROPERTY(Transient)
	TObjectPtr<UMosesCombatComponent> CachedCombatComponent = nullptr;
};
