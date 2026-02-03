#include "UE5_Multi_Shooter/System/MosesPawnSSOTGuardComponent.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Components/ActorComponent.h"

UMosesPawnSSOTGuardComponent::UMosesPawnSSOTGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesPawnSSOTGuardComponent::BeginPlay()
{
	Super::BeginPlay();

	ValidateOwnerClassProperties();
}

bool UMosesPawnSSOTGuardComponent::IsCosmeticAllowedProperty(const FProperty* Prop) const
{
	// 개발자 주석:
	// - Pawn은 "코스메틱 표현"을 담당할 수 있다.
	// - 따라서 AnimMontage/Sound/Niagara/Mesh/Material/Texture 같은 자산 레퍼런스는 허용한다.
	// - 또한 Pawn이 소유하는 컴포넌트 포인터(카메라/메시/컴포넌트)는 SSOT가 아니므로 허용한다.

	if (!Prop)
	{
		return false;
	}

	const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop);
	if (!ObjProp)
	{
		return false;
	}

	UClass* PropertyClass = ObjProp->PropertyClass;
	if (!PropertyClass)
	{
		return false;
	}

	// 컴포넌트 포인터는 허용(표현/구성 요소)
	if (PropertyClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return true;
	}

	// 애니/사운드/VFX (DAY3 코스메틱 핵심)
	if (PropertyClass->IsChildOf(UAnimMontage::StaticClass()))
	{
		return true;
	}
	if (PropertyClass->IsChildOf(USoundBase::StaticClass()))
	{
		return true;
	}
	if (PropertyClass->IsChildOf(UNiagaraSystem::StaticClass()))
	{
		return true;
	}

	// 메시/머티리얼/텍스처 등 렌더 자산 허용
	if (PropertyClass->IsChildOf(USkeletalMesh::StaticClass()))
	{
		return true;
	}
	if (PropertyClass->IsChildOf(UStaticMesh::StaticClass()))
	{
		return true;
	}
	if (PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
	{
		return true;
	}
	if (PropertyClass->IsChildOf(UTexture::StaticClass()))
	{
		return true;
	}

	return false;
}

bool UMosesPawnSSOTGuardComponent::IsForbiddenPropertyName(const FString& PropertyName) const
{
	// 개발자 주석:
	// - "Death" / "Kill" 같은 단어는 코스메틱(DeathMontage, KillSFX 등)에도 등장하므로 오탐이 잦다.
	// - 그래서 너무 일반적인 키워드는 금지 목록에서 제거하고,
	//   상태를 의미하는 명확한 토큰(Deaths, KillCount, DeathCount 등)만 잡는다.
	//
	// - 이름에 포함만 되어도 경고 대상으로 잡되, 코스메틱 자산은 IsCosmeticAllowedProperty에서 예외 처리한다.

	static const TCHAR* ForbiddenKeys[] =
	{
		// HP / Health
		TEXT("HP"), TEXT("Health"),

		// Ammo
		TEXT("Ammo"), TEXT("Bullet"), TEXT("Magazine"), TEXT("Reserve"),

		// Score / KDA (명확한 상태 토큰만)
		TEXT("Score"),
		TEXT("Kills"), TEXT("KillCount"),
		TEXT("Deaths"), TEXT("DeathCount"),

		// Shield (프로젝트가 사용하는 경우)
		TEXT("Shield")
	};

	for (const TCHAR* Key : ForbiddenKeys)
	{
		if (PropertyName.Contains(Key, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void UMosesPawnSSOTGuardComponent::ValidateOwnerClassProperties() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const UClass* OwnerClass = Owner->GetClass();
	if (!OwnerClass)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(OwnerClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* Prop = *It;
		if (!Prop)
		{
			continue;
		}

		const FString PropName = Prop->GetName();

		// 1) 코스메틱 자산/표현용 프로퍼티는 SSOT 검사에서 제외
		if (IsCosmeticAllowedProperty(Prop))
		{
			continue;
		}

		// 2) 이름 기반으로 SSOT 의심 키워드 검사
		if (!IsForbiddenPropertyName(PropName))
		{
			continue;
		}

		UE_LOG(LogMosesAuth, Error,
			TEXT("%s Pawn SSOT VIOLATION: Forbidden property '%s' found on PawnClass=%s Owner=%s"),
			MOSES_TAG_BLOCKED_CL,
			*PropName,
			*GetNameSafe(OwnerClass),
			*GetNameSafe(Owner));

		// 개발자 주석:
		// - 포트폴리오/증거 목적상 "실수는 즉시 멈춰서" 잡는 게 유리하다.
		// - 다만 너무 과격하면 개발 흐름이 끊기므로, 위 예외 처리로 오탐을 제거한 상태에서만 강제 중단한다.
		ensureAlwaysMsgf(false,
			TEXT("Pawn SSOT violation: '%s' must not exist on Pawn. Move it to PlayerState/CombatComponent."),
			*PropName);
	}
}
