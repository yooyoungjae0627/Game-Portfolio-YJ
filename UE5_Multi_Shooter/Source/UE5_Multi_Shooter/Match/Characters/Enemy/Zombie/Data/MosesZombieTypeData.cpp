#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Data/MosesZombieTypeData.h"

UMosesZombieTypeData::UMosesZombieTypeData()
{
	MaxHP = 100.f;
	MeleeDamage = 10.f;

	HeadshotAnnouncementText = FText::FromString(TEXT("헤드샷!"));
	HeadshotAnnouncementSeconds = 0.9f;

	// [MOD] AI defaults
	SightRadius = 1500.f;
	LoseSightRadius = 1800.f;
	PeripheralVisionAngleDeg = 70.f;

	AcceptanceRadius = 120.f;
	MaxChaseDistance = 4000.f;

	AttackRange = 180.f;
	AttackCooldownSeconds = 1.2f;
}
