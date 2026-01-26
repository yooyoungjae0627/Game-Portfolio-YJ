#include "UE5_Multi_Shooter/Pickup/Pickup_Ammo.h"

#include "GameFramework/Pawn.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Combat/MosesCombatComponent.h"

APickup_Ammo::APickup_Ammo()
{
}

bool APickup_Ammo::Server_ApplyPickup(APawn* InstigatorPawn)
{
	check(HasAuthority());

	//if (!IsValid(InstigatorPawn))
	//{
	//	return false;
	//}

	//AMosesPlayerState* PS = InstigatorPawn->GetPlayerState<AMosesPlayerState>();
	//if (!PS)
	//{
	//	UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=NoPlayerState Pawn=%s"), *GetNameSafe(InstigatorPawn));
	//	return false;
	//}

	//UMosesCombatComponent* Combat = PS->FindComponentByClass<UMosesCombatComponent>();
	//if (!Combat)
	//{
	//	UE_LOG(LogMosesPickup, Warning, TEXT("[PICKUP] FAIL Reason=NoCombatComponent PS=%s"), *GetNameSafe(PS));
	//	return false;
	//}

	//Combat->Server_AddReserveAmmo(WeaponType, AddReserveAmmo);
	return true;
}
