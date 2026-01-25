#include "UE5_Multi_Shooter/Development/Components/MosesAssetBootstrapComponent.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Weapons/MosesWeaponPreviewActor.h"

// 네 프로젝트 권한 가드 사용(업로드 파일이 만료될 수 있어도, 경로는 동일하게 유지)
// 만약 include 경로가 다르면 너 프로젝트 실제 경로에 맞춰 조정하면 됨.
#include "UE5_Multi_Shooter/System/MosesAuthorityGuards.h"

UMosesAssetBootstrapComponent::UMosesAssetBootstrapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesAssetBootstrapComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bValidateOnBeginPlay)
	{
		ValidateAssets();
	}

	if (bAutoBootstrapOnBeginPlay)
	{
		BootstrapWeapon_ServerOnly();
	}

	LogReadyOnce();
}

void UMosesAssetBootstrapComponent::ValidateAssets()
{
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s ValidateAssets: Owner is not ACharacter (%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(GetOwner()));
		return;
	}

	USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s ValidateAssets: Character Mesh null (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
		return;
	}

	if (MeshComp->GetSkeletalMeshAsset())
	{
		UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s CharacterMesh Ready: %s"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*MeshComp->GetSkeletalMeshAsset()->GetPathName());
	}
	else
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s CharacterMesh NOT set (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
	}

	if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
	{
		UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s AnimInstance Ready: %s (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(AnimInst->GetClass()), *GetNameSafe(OwnerCharacter));
	}
	else
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s AnimInstance NULL (AnimBP not set?) Owner=%s"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
	}

	if (!MeshComp->DoesSocketExist(CharacterWeaponSocketName))
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Missing Character Socket: %s (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*CharacterWeaponSocketName.ToString(), *GetNameSafe(OwnerCharacter));
	}
	else
	{
		UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s Character Socket OK: %s (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*CharacterWeaponSocketName.ToString(), *GetNameSafe(OwnerCharacter));
	}

	// 몽타주는 DAY0 스모크용: 미지정이면 경고만
	if (!HitReactMontage)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s HitReactMontage not set (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
	}

	if (!DeathMontage)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s DeathMontage not set (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
	}
}

void UMosesAssetBootstrapComponent::BootstrapWeapon_ServerOnly()
{
	// 서버 권위 가드(클라에서 호출되면 BLOCK 로그를 남기는 게 프로젝트 원칙)
	MOSES_GUARD_AUTHORITY_VOID(this, "Asset", TEXT("BootstrapWeapon_ServerOnly is server-only"));

	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	if (SpawnedWeapon)
	{
		return;
	}

	if (!DefaultWeaponClass)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s DefaultWeaponClass not set (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
		return;
	}

	USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Owner Mesh null, cannot attach (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = OwnerCharacter;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedWeapon = World->SpawnActor<AMosesWeaponPreviewActor>(DefaultWeaponClass, FTransform::Identity, Params);
	if (!SpawnedWeapon)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Weapon spawn failed (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(OwnerCharacter));
		return;
	}

	SpawnedWeapon->ValidateWeaponSockets(true);
	SpawnedWeapon->AttachToCharacterMesh(MeshComp, CharacterWeaponSocketName);

	UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s Weapon bootstrap OK (Owner=%s Weapon=%s)"),
		MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
		*GetNameSafe(OwnerCharacter), *GetNameSafe(SpawnedWeapon));
}

void UMosesAssetBootstrapComponent::PlayHitReact()
{
	PlayMontageIfPossible(HitReactMontage, TEXT("HitReact"));
}

void UMosesAssetBootstrapComponent::PlayDeath()
{
	PlayMontageIfPossible(DeathMontage, TEXT("Death"));
}

void UMosesAssetBootstrapComponent::PlayAttack()
{
	PlayMontageIfPossible(AttackMontage, TEXT("Attack"));
}

ACharacter* UMosesAssetBootstrapComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

void UMosesAssetBootstrapComponent::LogReadyOnce()
{
	if (bLoggedReady)
	{
		return;
	}

	UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s Character/Zombie/Weapons ready (Owner=%s)"),
		MOSES_TAG_ASSET_SV,
		MosesLog::GetWorldNameSafe(this),
		MosesLog::GetNetModeNameSafe(this),
		*GetNameSafe(GetOwner()));

	bLoggedReady = true;
}

void UMosesAssetBootstrapComponent::PlayMontageIfPossible(UAnimMontage* Montage, const TCHAR* DebugName)
{
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s PlayMontage(%s): Mesh null (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			DebugName, *GetNameSafe(OwnerCharacter));
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s PlayMontage(%s): AnimInstance null (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			DebugName, *GetNameSafe(OwnerCharacter));
		return;
	}

	if (!Montage)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s PlayMontage(%s): Montage not set (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			DebugName, *GetNameSafe(OwnerCharacter));
		return;
	}

	const float PlayedLen = AnimInst->Montage_Play(Montage, 1.0f);
	if (PlayedLen <= 0.f)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Montage_Play failed (%s) (Owner=%s)"),
			MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
			DebugName, *GetNameSafe(OwnerCharacter));
		return;
	}

	UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s Montage_Play OK (%s) Owner=%s Montage=%s"),
		MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
		DebugName, *GetNameSafe(OwnerCharacter), *GetNameSafe(Montage));
}
