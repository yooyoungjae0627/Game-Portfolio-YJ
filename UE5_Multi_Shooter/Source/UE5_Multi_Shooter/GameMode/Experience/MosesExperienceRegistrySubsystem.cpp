#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceRegistrySubsystem.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"

void UMosesExperienceRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMosesExperienceRegistrySubsystem::SetFromExperience(const UMosesExperienceDefinition* Experience)
{
	/**
	 * 개발자 주석:
	 * - Registry는 "현재 Experience가 요구하는 Payload"를 보관한다.
	 * - 실제 적용은 Applicator(로컬)에서 수행한다.
	 */
	if (!Experience)
	{
		Clear();
		return;
	}

	StartWidgetClass = Experience->GetStartWidgetClass();
	HUDWidgetClass = Experience->GetHUDWidgetClass();

	InputMapping = Experience->GetInputMapping();
	InputPriority = Experience->GetInputPriority();

	AbilitySetPath = Experience->GetAbilitySetPath();
}

void UMosesExperienceRegistrySubsystem::Clear()
{
	StartWidgetClass.Reset();
	HUDWidgetClass.Reset();

	InputMapping.Reset();
	InputPriority = 100;

	AbilitySetPath.Reset();
}
