#pragma once

#include "GameFramework/Actor.h"
#include "MosesWeaponPreviewActor.generated.h"

class USkeletalMeshComponent;

/**
 * AMosesWeaponPreviewActor
 *
 * - 무기 메시/소켓 규칙을 코드로 "고정"한다.
 * - 캐릭터의 WeaponSocket(hand_r)에 부착되는 최소 무기 Actor.
 *
 * [소켓 규칙 - 무기 SkeletalMesh]
 * - Muzzle
 * - ShellEject
 * - Grip (선택)
 *
 * [제공 기능]
 * - 소켓 Transform(World) 반환
 * - 소켓 존재 검증(로그)
 * - 캐릭터 Mesh에 Attach
 *
 * [주의]
 * - 전투(발사/탄약/데미지)는 포함하지 않는다.
 */
UCLASS(Blueprintable)
class UE5_MULTI_SHOOTER_API AMosesWeaponPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AMosesWeaponPreviewActor();

public:
	UFUNCTION(BlueprintCallable, Category="Moses|Weapon")
	FTransform GetMuzzleTransform_World() const;

	UFUNCTION(BlueprintCallable, Category="Moses|Weapon")
	FTransform GetShellEjectTransform_World() const;

	UFUNCTION(BlueprintCallable, Category="Moses|Weapon")
	FTransform GetGripTransform_World() const;

public:
	UFUNCTION(BlueprintCallable, Category="Moses|Weapon")
	void AttachToCharacterMesh(USkeletalMeshComponent* CharacterMesh, FName CharacterWeaponSocketName);

	UFUNCTION(BlueprintCallable, Category="Moses|Weapon")
	bool ValidateWeaponSockets(bool bLogIfMissing) const;

public:
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

protected:
	virtual void BeginPlay() override;

private:
	FTransform GetSocketTransformSafe_World(FName SocketName) const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Moses|Weapon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

private:
	static const FName Socket_Muzzle;
	static const FName Socket_ShellEject;
	static const FName Socket_Grip;
};
