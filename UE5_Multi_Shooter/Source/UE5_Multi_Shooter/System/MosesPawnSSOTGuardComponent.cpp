#include "UE5_Multi_Shooter/System/MosesPawnSSOTGuardComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

UMosesPawnSSOTGuardComponent::UMosesPawnSSOTGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMosesPawnSSOTGuardComponent::BeginPlay()
{
	Super::BeginPlay();

	ValidateOwnerClassProperties();
}

bool UMosesPawnSSOTGuardComponent::IsForbiddenPropertyName(const FString& PropertyName) const
{
	// 개발자 주석:
	// - Pawn에 있으면 안 되는 SSOT 데이터 키워드들.
	// - 이름에 포함만 되어도 경고 대상으로 잡는다(실수 방지 목적).
	static const TCHAR* ForbiddenKeys[] =
	{
		TEXT("HP"), TEXT("Health"),
		TEXT("Ammo"), TEXT("Bullet"),
		TEXT("Score"), TEXT("Kill"), TEXT("Death"),
		TEXT("Kills"), TEXT("Deaths")
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

		ensureAlwaysMsgf(false,
			TEXT("Pawn SSOT violation: '%s' must not exist on Pawn. Move it to PlayerState/CombatComponent."),
			*PropName);
	}
}
