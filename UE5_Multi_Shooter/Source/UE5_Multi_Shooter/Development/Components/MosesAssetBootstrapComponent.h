#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MosesAssetBootstrapComponent.generated.h"

class AMosesWeaponPreviewActor;
class UAnimMontage;
class ACharacter;

/**
 * UMosesAssetBootstrapComponent
 *
 * - “기본 재료” 확정을 코드 레벨에서 강제한다.
 *
 * [기능]
 * 1) 서버 권위로 기본 무기 스폰 → 캐릭터 WeaponSocket에 Attach
 * 2) 캐릭터/무기 소켓 규칙 검증(로그)
 * 3) HitReact/Death/Attack 몽타주 수동 재생(스모크 테스트)
 * 4) 증거 로그: "[ASSET][SV] ... World/NetMode" 포함
 *
 * [사용]
 * - Character BP / Zombie BP에 Add Component
 * - DefaultWeaponClass 지정(부모: AMosesWeaponPreviewActor)
 * - 몽타주 1~3개 지정(선택)
 */
UCLASS(ClassGroup=(Moses), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesAssetBootstrapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesAssetBootstrapComponent();

public:
	UFUNCTION(BlueprintCallable, Category="Moses|Development|Assets")
	void ValidateAssets();

	UFUNCTION(BlueprintCallable, Category="Moses|Development|Assets")
	void BootstrapWeapon_ServerOnly();

public:
	UFUNCTION(BlueprintCallable, Category="Moses|Development|Anim")
	void PlayHitReact();

	UFUNCTION(BlueprintCallable, Category="Moses|Development|Anim")
	void PlayDeath();

	UFUNCTION(BlueprintCallable, Category="Moses|Development|Anim")
	void PlayAttack();

public:
	AMosesWeaponPreviewActor* GetSpawnedWeapon() const { return SpawnedWeapon; }

protected:
	virtual void BeginPlay() override;

private:
	ACharacter* GetOwnerCharacter() const;
	void LogReadyOnce();
	void PlayMontageIfPossible(UAnimMontage* Montage, const TCHAR* DebugName);

private:
	UPROPERTY(EditAnywhere, Category="Moses|Development|Bootstrap", meta=(AllowPrivateAccess="true"))
	bool bAutoBootstrapOnBeginPlay = true;

	UPROPERTY(EditAnywhere, Category="Moses|Development|Bootstrap", meta=(AllowPrivateAccess="true"))
	TSubclassOf<AMosesWeaponPreviewActor> DefaultWeaponClass;

	UPROPERTY(EditAnywhere, Category="Moses|Development|Sockets", meta=(AllowPrivateAccess="true"))
	FName CharacterWeaponSocketName = TEXT("WeaponSocket");

	UPROPERTY(EditAnywhere, Category="Moses|Development|Validation", meta=(AllowPrivateAccess="true"))
	bool bValidateOnBeginPlay = true;

private:
	UPROPERTY(EditAnywhere, Category="Moses|Development|Anim", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditAnywhere, Category="Moses|Development|Anim", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, Category="Moses|Development|Anim", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> AttackMontage;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMosesWeaponPreviewActor> SpawnedWeapon = nullptr;

	UPROPERTY(Transient)
	bool bLoggedReady = false;
};
