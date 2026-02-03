#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"

UMosesZombieTypeData::UMosesZombieTypeData()
{
	MaxHP = 100.f;
	MeleeDamage = 10.f;

	HeadshotAnnouncementText = FText::FromString(TEXT("헤드샷!"));
	HeadshotAnnouncementSeconds = 0.9f;
}
