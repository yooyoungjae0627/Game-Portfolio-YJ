#include "UE5_Multi_Shooter/Weapons/MosesWeaponPreviewActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

const FName AMosesWeaponPreviewActor::Socket_Muzzle(TEXT("Muzzle"));
const FName AMosesWeaponPreviewActor::Socket_ShellEject(TEXT("ShellEject"));
const FName AMosesWeaponPreviewActor::Socket_Grip(TEXT("Grip"));

AMosesWeaponPreviewActor::AMosesWeaponPreviewActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
}

void AMosesWeaponPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponMesh && WeaponMesh->GetSkeletalMeshAsset())
	{
		UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s WeaponMesh Ready: %s"),
			MOSES_TAG_ASSET_SV,
			MosesLog::GetWorldNameSafe(this),
			MosesLog::GetNetModeNameSafe(this),
			*WeaponMesh->GetSkeletalMeshAsset()->GetPathName());
	}
	else
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s WeaponMesh NOT set (Weapon=%s)"),
			MOSES_TAG_ASSET_SV,
			MosesLog::GetWorldNameSafe(this),
			MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(this));
	}
}

FTransform AMosesWeaponPreviewActor::GetMuzzleTransform_World() const
{
	return GetSocketTransformSafe_World(Socket_Muzzle);
}

FTransform AMosesWeaponPreviewActor::GetShellEjectTransform_World() const
{
	return GetSocketTransformSafe_World(Socket_ShellEject);
}

FTransform AMosesWeaponPreviewActor::GetGripTransform_World() const
{
	return GetSocketTransformSafe_World(Socket_Grip);
}

void AMosesWeaponPreviewActor::AttachToCharacterMesh(USkeletalMeshComponent* CharacterMesh, FName CharacterWeaponSocketName)
{
	if (!CharacterMesh || !WeaponMesh)
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Attach failed. CharacterMesh=%s WeaponMesh=%s Weapon=%s"),
			MOSES_TAG_ASSET_SV,
			MosesLog::GetWorldNameSafe(this),
			MosesLog::GetNetModeNameSafe(this),
			*GetNameSafe(CharacterMesh),
			*GetNameSafe(WeaponMesh),
			*GetNameSafe(this));
		return;
	}

	if (!CharacterMesh->DoesSocketExist(CharacterWeaponSocketName))
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Missing Character Socket: %s (CharacterMesh=%s)"),
			MOSES_TAG_ASSET_SV,
			MosesLog::GetWorldNameSafe(this),
			MosesLog::GetNetModeNameSafe(this),
			*CharacterWeaponSocketName.ToString(),
			*GetNameSafe(CharacterMesh));
	}

	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(CharacterMesh, Rules, CharacterWeaponSocketName);

	UE_LOG(LogMosesAsset, Log, TEXT("%s %s %s Weapon Attached: %s -> %s"),
		MOSES_TAG_ASSET_SV,
		MosesLog::GetWorldNameSafe(this),
		MosesLog::GetNetModeNameSafe(this),
		*GetNameSafe(this),
		*CharacterWeaponSocketName.ToString());
}

bool AMosesWeaponPreviewActor::ValidateWeaponSockets(bool bLogIfMissing) const
{
	if (!WeaponMesh)
	{
		if (bLogIfMissing)
		{
			UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s ValidateWeaponSockets: WeaponMesh null (Weapon=%s)"),
				MOSES_TAG_ASSET_SV,
				MosesLog::GetWorldNameSafe(this),
				MosesLog::GetNetModeNameSafe(this),
				*GetNameSafe(this));
		}
		return false;
	}

	const bool bHasMuzzle = WeaponMesh->DoesSocketExist(Socket_Muzzle);
	const bool bHasShell  = WeaponMesh->DoesSocketExist(Socket_ShellEject);
	const bool bHasGrip   = WeaponMesh->DoesSocketExist(Socket_Grip); // 선택

	if (bLogIfMissing)
	{
		if (!bHasMuzzle)
		{
			UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Missing Weapon Socket: %s (Weapon=%s)"),
				MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
				*Socket_Muzzle.ToString(), *GetNameSafe(this));
		}

		if (!bHasShell)
		{
			UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Missing Weapon Socket: %s (Weapon=%s)"),
				MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
				*Socket_ShellEject.ToString(), *GetNameSafe(this));
		}

		if (!bHasGrip)
		{
			UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Missing Weapon Socket: %s (Weapon=%s)"),
				MOSES_TAG_ASSET_SV, MosesLog::GetWorldNameSafe(this), MosesLog::GetNetModeNameSafe(this),
				*Socket_Grip.ToString(), *GetNameSafe(this));
		}
	}

	// DAY0 통과 기준: Muzzle + ShellEject 필수, Grip은 선택.
	return (bHasMuzzle && bHasShell);
}

FTransform AMosesWeaponPreviewActor::GetSocketTransformSafe_World(FName SocketName) const
{
	if (!WeaponMesh)
	{
		return FTransform::Identity;
	}

	if (!WeaponMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogMosesAsset, Warning, TEXT("%s %s %s Missing Weapon Socket: %s (Weapon=%s)"),
			MOSES_TAG_ASSET_SV,
			MosesLog::GetWorldNameSafe(this),
			MosesLog::GetNetModeNameSafe(this),
			*SocketName.ToString(),
			*GetNameSafe(this));

		return WeaponMesh->GetComponentTransform();
	}

	return WeaponMesh->GetSocketTransform(SocketName, RTS_World);
}
