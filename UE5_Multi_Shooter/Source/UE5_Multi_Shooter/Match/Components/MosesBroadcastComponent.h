#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "MosesBroadcastComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMosesBroadcastChangedNative, const FString& /*Message*/, float /*DisplaySeconds*/);

USTRUCT()
struct FMosesBroadcastMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Message;

	UPROPERTY()
	float DisplaySeconds = 2.5f;
};

UCLASS(ClassGroup=(Moses), meta=(BlueprintSpawnableComponent))
class UE5_MULTI_SHOOTER_API UMosesBroadcastComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMosesBroadcastComponent();

	// 서버: 방송 확정
	void ServerBroadcastMessage(const FString& InMessage, float InDisplaySeconds);

	// HUD 바인딩
	FOnMosesBroadcastChangedNative OnBroadcastChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Broadcast();

	void BroadcastToUI();

	UPROPERTY(ReplicatedUsing=OnRep_Broadcast)
	FMosesBroadcastMessage Broadcast;
};
