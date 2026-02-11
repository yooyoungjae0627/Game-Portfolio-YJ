#include "UE5_Multi_Shooter/MosesGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h"

AMosesGameState::AMosesGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// [핵심]
	// GameModeBase::InitGameState()에서 반드시 존재해야 하므로
	// GameState 생성자에서 기본 서브오브젝트로 고정한다.
	ExperienceManagerComponent =
		CreateDefaultSubobject<UMosesExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));

	bReplicates = true;

	// 서버 기본: Lobby에서 시작하는 플로우가 일반적이므로 기본값을 Lobby로 둔다.
	// (MatchLevel에서 바로 시작하는 경우, GameMode에서 ServerSetPhase로 덮어써도 된다.)
	CurrentPhase = EMosesServerPhase::Lobby;
}

void AMosesGameState::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMosesExp, Warning,
		TEXT("[GameStateBase] BeginPlay World=%s NetMode=%d Phase=%s ExpComp=%s"),
		*GetNameSafe(GetWorld()),
		(int32)GetNetMode(),
		*UEnum::GetValueAsString(CurrentPhase),
		*GetNameSafe(ExperienceManagerComponent));
}

void AMosesGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesGameState, CurrentPhase);
}

void AMosesGameState::OnRep_CurrentPhase()
{
	// 필요 시: 로비 UI나 매치 UI에서 전역 Phase 변화 감지 포인트로 사용 가능.
}

void AMosesGameState::ServerSetPhase(EMosesServerPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	UE_LOG(LogMosesPhase, Log, TEXT("[PHASE][SV] GlobalPhase=%s"), *UEnum::GetValueAsString(CurrentPhase));

	// 리슨서버 즉시 반영용
	OnRep_CurrentPhase();
}
