#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesBroadcastComponent.h"

UMosesBroadcastComponent::UMosesBroadcastComponent()
{
	SetIsReplicatedByDefault(true);
}

void UMosesBroadcastComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesBroadcastComponent, Broadcast);
}

void UMosesBroadcastComponent::ServerBroadcastMessage(const FString& InMessage, float InDisplaySeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Broadcast.Message = InMessage;
	Broadcast.DisplaySeconds = FMath::Max(0.1f, InDisplaySeconds);

	UE_LOG(LogMosesMatch, Log, TEXT("[BROADCAST][SV] Msg='%s' Sec=%.2f"), *Broadcast.Message, Broadcast.DisplaySeconds);

	BroadcastToUI(); // 서버도 즉시
}

void UMosesBroadcastComponent::OnRep_Broadcast()
{
	UE_LOG(LogMosesMatch, Verbose, TEXT("[BROADCAST][CL] OnRep Msg='%s' Sec=%.2f"), *Broadcast.Message, Broadcast.DisplaySeconds);
	BroadcastToUI();
}

void UMosesBroadcastComponent::BroadcastToUI()
{
	OnBroadcastChanged.Broadcast(Broadcast.Message, Broadcast.DisplaySeconds);
}
