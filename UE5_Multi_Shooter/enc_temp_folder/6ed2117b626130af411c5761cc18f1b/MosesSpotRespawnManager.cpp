// ============================================================================
// MosesSpotRespawnManager.cpp (FULL)  [MOD]
// ============================================================================

#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Actor/MosesSpotRespawnManager.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"
#include "UE5_Multi_Shooter/Match/Characters/Enemy/Zombie/Actor/MosesZombieSpawnSpot.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AMosesSpotRespawnManager::AMosesSpotRespawnManager()
{
	bReplicates = false;
	SetActorTickEnabled(false);
}

void AMosesSpotRespawnManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// [MOD] 권한 경계 증거 로그(클라는 매니저가 서버전용임)
		UE_LOG(LogMosesAnnounce, Verbose, TEXT("[ANN][CL] RespawnManager BeginPlay (Ignored - ServerOnly)"));
		return;
	}

	// 디버그/검증용: ManagedSpots 비었으면 월드에서 스캔
	if (ManagedSpots.Num() <= 0)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AMosesZombieSpawnSpot> It(World); It; ++It)
			{
				ManagedSpots.Add(*It);
			}
		}
	}

	UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnManager Ready Spots=%d"), ManagedSpots.Num());
}

void AMosesSpotRespawnManager::ServerOnSpotCaptured(AMosesZombieSpawnSpot* OldZoneSpotToRespawn) // [MOD]
{
	// [MOD] BP에서 AuthorityOnly라도, C++에서 오호출 방지 및 증거 로그
	if (!HasAuthority())
	{
		UE_LOG(LogMosesAnnounce, Warning,
			TEXT("[ANN][CL] ServerOnSpotCaptured called on Client (Ignored) Spot=%s"),
			*GetNameSafe(OldZoneSpotToRespawn));
		return;
	}

	if (!OldZoneSpotToRespawn)
	{
		UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnCountdown Start FAIL (NullSpot)"));
		return;
	}

	StartCountdown_Server(OldZoneSpotToRespawn);
}

void AMosesSpotRespawnManager::StartCountdown_Server(AMosesZombieSpawnSpot* TargetSpot)
{
	check(HasAuthority());

	if (!ensureMsgf(TargetSpot, TEXT("[ANN][SV] StartCountdown_Server TargetSpot is NULL")))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!ensureMsgf(World, TEXT("[ANN][SV] StartCountdown_Server World is NULL")))
	{
		return;
	}

	// 진행 중이던 카운트다운이 있으면 최신으로 교체
	const bool bWasActive = GetWorldTimerManager().IsTimerActive(CountdownTimerHandle);

	PendingRespawnSpot = TargetSpot;

	const float Now = World->GetTimeSeconds();
	RespawnEndServerTime = Now + static_cast<float>(RespawnCountdownSeconds);

	UE_LOG(LogMosesAnnounce, Warning,
		TEXT("[ANN][SV] RespawnCountdown Start Spot=%s EndTime=%.2f WasActive=%d Seconds=%d"),
		*GetNameSafe(PendingRespawnSpot),
		RespawnEndServerTime,
		bWasActive ? 1 : 0,
		RespawnCountdownSeconds);

	// [MOD] 기존 타이머 정리 후 즉시 1회 방송(10초부터 보여주기)
	StopCountdown_Server();
	ServerTickRespawnCountdown();

	// 1초 타이머
	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ThisClass::ServerTickRespawnCountdown,
		1.0f,
		true);
}

void AMosesSpotRespawnManager::StopCountdown_Server()
{
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
}

void AMosesSpotRespawnManager::ServerTickRespawnCountdown()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!PendingRespawnSpot)
	{
		UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnCountdown Tick FAIL (PendingRespawnSpot NULL)"));
		StopCountdown_Server();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnCountdown Tick FAIL (World NULL)"));
		StopCountdown_Server();
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Remaining = RespawnEndServerTime - Now;

	// 남은 초 정수화 (10→0)
	const int32 RemainingSec = FMath::Max(0, FMath::CeilToInt(Remaining));

	UE_LOG(LogMosesAnnounce, Warning,
		TEXT("[ANN][SV] RespawnCountdown Tick Spot=%s Remaining=%d"),
		*GetNameSafe(PendingRespawnSpot),
		RemainingSec);

	// ---------------------------------------------------------------------
	// [MOD] 깜빡임 방지: "남은 초가 바뀔 때만" 브로드캐스트한다.
	// - 기존 코드는 매 Tick마다 StartText(1초)를 다시 시작해서 UI가 리셋 → 깜빡임
	// ---------------------------------------------------------------------
	if (RemainingSec != LastBroadcastRemainingSec)
	{
		LastBroadcastRemainingSec = RemainingSec;
		BroadcastCountdown_Server(RemainingSec);
	}

	if (RemainingSec <= 0)
	{
		ExecuteCountdown_Server();
	}
}

void AMosesSpotRespawnManager::BroadcastCountdown_Server(int32 RemainingSec)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesMatchGameState* MGS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr;
	if (!MGS)
	{
		UE_LOG(LogMosesAnnounce, Verbose, TEXT("[ANN][SV] BroadcastCountdown Skip (MGS NULL)"));
		return;
	}

	// [MOD] "Start(새 공지)"를 매초 호출하지 말고,
	//       "카운트다운 공지" 상태를 In-Place로 업데이트한다.
	MGS->ServerSetCountdownAnnouncement(RemainingSec);
}

void AMosesSpotRespawnManager::ExecuteCountdown_Server()
{
	check(HasAuthority());

	StopCountdown_Server();

	if (!PendingRespawnSpot)
	{
		UE_LOG(LogMosesAnnounce, Warning, TEXT("[ANN][SV] RespawnCountdown Execute FAIL (PendingRespawnSpot NULL)"));
		return;
	}

	UE_LOG(LogMosesAnnounce, Warning,
		TEXT("[ANN][SV] RespawnCountdown Execute Spot=%s"),
		*GetNameSafe(PendingRespawnSpot));

	// ✅ STEP3 DoD의 ZOMBIE 로그는 SpawnSpot 내부에서 찍힘
	PendingRespawnSpot->ServerRespawnSpotZombies();

	PendingRespawnSpot = nullptr;
	RespawnEndServerTime = 0.f;
}
