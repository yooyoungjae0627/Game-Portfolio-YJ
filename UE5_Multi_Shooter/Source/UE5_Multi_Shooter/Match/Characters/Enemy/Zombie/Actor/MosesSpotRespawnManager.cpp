#include "MosesSpotRespawnManager.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/MosesZombieCharacter.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Actor/MosesZombieSpawnSpot.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMosesSpotRespawnManager::AMosesSpotRespawnManager()
{
	bReplicates = false;
	RespawnCountdownSeconds = 10;
	CurrentCountdown = 0;
}

void AMosesSpotRespawnManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (ManagedSpots.Num() <= 0)
	{
		for (TActorIterator<AMosesZombieSpawnSpot> It(GetWorld()); It; ++It)
		{
			ManagedSpots.Add(*It);
		}
	}

	UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnManager Ready Spots=%d"), ManagedSpots.Num());
}

void AMosesSpotRespawnManager::ServerOnSpotCaptured(AMosesZombieSpawnSpot* CapturedSpot)
{
	if (!HasAuthority() || !CapturedSpot)
	{
		return;
	}

	PendingRespawnSpot = CapturedSpot;
	CurrentCountdown = RespawnCountdownSeconds;

	UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnCountdown Start=%d Spot=%s"),
		CurrentCountdown, *GetNameSafe(PendingRespawnSpot));

	BroadcastCountdown_Server(CurrentCountdown);

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&AMosesSpotRespawnManager::TickCountdown_Server,
		1.0f,
		true);
}

void AMosesSpotRespawnManager::TickCountdown_Server()
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentCountdown = FMath::Max(0, CurrentCountdown - 1);
	BroadcastCountdown_Server(CurrentCountdown);

	if (CurrentCountdown <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		ExecuteRespawn_Server();
	}
}

void AMosesSpotRespawnManager::BroadcastCountdown_Server(int32 Seconds)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
	if (!MGS)
	{
		return;
	}

	const FString Msg = FString::Printf(TEXT("Respawn in %d"), Seconds);
	MGS->ServerPushAnnouncement(Msg, 1.1f);

	UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnCountdown Tick=%d Spot=%s"), Seconds, *GetNameSafe(PendingRespawnSpot));
}

void AMosesSpotRespawnManager::ExecuteRespawn_Server()
{
	if (!HasAuthority() || !PendingRespawnSpot)
	{
		return;
	}

	UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] Respawn Execute Spot=%s"), *GetNameSafe(PendingRespawnSpot));

	PendingRespawnSpot->ServerRespawnSpotZombies();

	// TODO(확장): Flag 리스폰이 필요하면 여기서 훅 호출
	// PendingRespawnSpot->ServerRespawnFlag();

	PendingRespawnSpot = nullptr;
}
