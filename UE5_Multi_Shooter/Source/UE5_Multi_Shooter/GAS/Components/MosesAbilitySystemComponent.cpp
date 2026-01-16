#include "MosesAbilitySystemComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

void UMosesAbilitySystemComponent::DumpOwnedTagsToLog() const
{
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);

	UE_LOG(LogMosesGAS, Log, TEXT("[GAS] DumpTags: %s"), *OwnedTags.ToStringSimple());
}
