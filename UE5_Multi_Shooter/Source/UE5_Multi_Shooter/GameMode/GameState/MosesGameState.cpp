#include "MosesGameState.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

AMosesGameState::AMosesGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Experience 로딩 담당 컴포넌트 생성
	ExperienceManagerComponent =
		CreateDefaultSubobject<UMosesExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));

	/**
	 * 서버 초기 상태 고정
	 * - Lobby에서 시작
	 * - SpawnGate 닫힘
	 * - Start 요청 없음
	 */
	CurrentPhase = EMosesServerPhase::Lobby;
	bSpawnGateOpen = false;
	bStartRequested = false;
}

void AMosesGameState::BeginPlay()
{
	Super::BeginPlay();

	// 서버 / 클라이언트 구분 확인용 로그
	UE_LOG(LogMosesExp, Warning,
		TEXT("[GS] BeginPlay World=%s NetMode=%d"),
		*GetNameSafe(GetWorld()),
		(int32)GetNetMode());
}

void AMosesGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 서버 상태를 클라이언트로 복제
	DOREPLIFETIME(AMosesGameState, CurrentPhase);
	DOREPLIFETIME(AMosesGameState, bSpawnGateOpen);
	DOREPLIFETIME(AMosesGameState, bStartRequested);
	DOREPLIFETIME(AMosesGameState, StartRequesterPid);
}

void AMosesGameState::OnRep_CurrentPhase()
{
	// 클라이언트 UI 갱신용
}

void AMosesGameState::OnRep_SpawnGateOpen()
{
	// 클라이언트 스폰 허용 여부 반영
}

void AMosesGameState::OnRep_StartRequested()
{
	// Start 버튼 UI 비활성화 등 처리 가능
}

void AMosesGameState::ServerSetPhase(EMosesServerPhase NewPhase)
{
	// 서버만 변경 가능
	if (!HasAuthority())
	{
		return;
	}

	// 중복 변경 방지
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	// 서버 로그 기준 고정 (DoD 증거)
	if (CurrentPhase == EMosesServerPhase::Lobby)
	{
		UE_LOG(LogMosesPhase, Log, TEXT("[PHASE] Current = Lobby"));
	}
	else if (CurrentPhase == EMosesServerPhase::Match)
	{
		UE_LOG(LogMosesPhase, Log, TEXT("[PHASE] Current = Match"));
	}
}

void AMosesGameState::ServerRequestStart_ReserveOnly(const FGuid& RequesterPid)
{
	if (!HasAuthority())
	{
		return;
	}

	/**
	 * 이미 Start가 눌렸다면 무시
	 * → 서버 단일 진실 유지
	 */
	if (bStartRequested)
	{
		UE_LOG(LogMosesPhase, Log,
			TEXT("[PHASE] Start Requested IGNORE (AlreadyRequested)"));
		return;
	}

	// Start 요청 기록
	bStartRequested = true;
	StartRequesterPid = RequesterPid;

	UE_LOG(LogMosesPhase, Log,
		TEXT("[PHASE] Start Requested by Host Pid=%s"),
		*RequesterPid.ToString(EGuidFormats::DigitsWithHyphensLower));

	/**
	 * Ready ≠ Spawn
	 * → Start를 눌러도 스폰은 절대 열리지 않는다
	 */
	ServerCloseSpawnGate();
}

void AMosesGameState::ServerCloseSpawnGate()
{
	if (!HasAuthority())
	{
		return;
	}

	// 이미 닫혀 있어도 로그는 남김 (판단 근거)
	if (!bSpawnGateOpen)
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[SPAWN] SpawnGate Closed"));
		return;
	}

	bSpawnGateOpen = false;
	UE_LOG(LogMosesSpawn, Log, TEXT("[SPAWN] SpawnGate Closed"));
}

bool AMosesGameState::IsStartRequested() const
{
	return bStartRequested;
}

bool AMosesGameState::IsSpawnGateOpen() const
{
	return bSpawnGateOpen;
}

EMosesServerPhase AMosesGameState::GetCurrentPhase() const
{
	return CurrentPhase;
}
