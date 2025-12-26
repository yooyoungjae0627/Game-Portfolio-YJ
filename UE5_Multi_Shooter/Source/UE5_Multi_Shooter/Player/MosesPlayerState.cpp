#include "MosesPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

#include "UE5_Multi_Shooter/Character/MosesPawnData.h"
#include "UE5_Multi_Shooter/GameMode/MosesExperienceManagerComponent.h"
#include "UE5_Multi_Shooter/GameMode/MosesGameModeBase.h"

AMosesPlayerState::AMosesPlayerState()
{
	// ctor에서는 Authority가 애매할 수 있어서 여기서 강제 생성하지 않는다.
	// PersistentId는 PostInitializeComponents에서 서버 권한일 때만 보장한다.
}

void AMosesPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsurePersistentId_ServerOnly();

	// Experience 로딩 완료 이벤트에 콜백 등록(이미 로드면 즉시 호출됨)
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	check(GameState);

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	check(ExperienceManagerComponent);

	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
		FOnMosesExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded)
	);
}

void AMosesPlayerState::EnsurePersistentId_ServerOnly()
{
	// 서버에서만 PersistentId를 생성/보장
	if (HasAuthority() && !PersistentId.IsValid())
	{
		PersistentId = FGuid::NewGuid();
	}
}

// ----------------------------------------------------
// Experience 로딩 완료 시점에 PlayerState가 해야 할 초기화
// ----------------------------------------------------
void AMosesPlayerState::OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience)
{
	// ※ 주의: 이 함수가 "호출 자체"는 서버/클라 어디서든 가능할 수 있음.
	// 하지만 아래 로직은 GameMode를 사용하므로 "서버 전용 로직"이 된다.

	// GetAuthGameMode는 서버에만 유효:
	// - GameMode는 서버에만 존재(Authority only)
	// - 클라이언트에서는 보통 nullptr
	if (AMosesGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AMosesGameModeBase>())
	{
		// PlayerState의 Owner는 보통 Controller(서버 기준: PlayerController)임
		AController* OwnerController = Cast<AController>(GetOwner()); // PlayerState Owner = Controller

		// GameMode 정책에 따라 "이 컨트롤러가 어떤 PawnData를 써야 하는지" 결정
		// (예: Experience/Team/캐릭터 선택 등에 따라 PawnData가 달라질 수 있음)
		const UMosesPawnData* NewPawnData = GameMode->GetPawnDataForController(OwnerController);

		// 서버에서 PawnData를 못 찾으면 스폰/능력치/입력 세팅이 깨지므로 강제 크래시로 조기 발견
		check(NewPawnData);

		// PlayerState에 PawnData를 1회 세팅 (정책상 한번만)
		SetPawnData(NewPawnData);

		UE_LOG(LogTemp, Warning,
			TEXT("[PS][PawnData] SetPawnData PS=%s PC=%s PawnData=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerController),
			*GetNameSafe(NewPawnData)
		);
	}
	else
	{
		// 공부 포인트:
		// 클라이언트에서는 여기로 떨어질 확률이 높다(= GameMode가 없기 때문).
		// 즉, 현재 구현은 "서버에서 PawnData를 세팅"하는 의도에 맞다.
		// (필요하면 여기에서 클라용 처리 대신, RepNotify(OnRep_PawnData)를 쓰는 게 정석)
	}
}

// ----------------------------------------------------
// PawnData 세팅 함수 (정책: 최초 1회만 세팅)
// ----------------------------------------------------
void AMosesPlayerState::SetPawnData(const UMosesPawnData* InPawnData)
{
	// PawnData는 게임 플레이 전반(능력/입력/카메라/애니 등)의 "설계도" 역할을 하므로 null이면 안 됨
	check(InPawnData);

	// 정책: 최초 1회만 설정
	// - Experience는 보통 매치 동안 고정이므로 "한 번만"이 합리적
	// - 하지만 맵 이동/Experience 전환을 지원할 거면 이 정책은 바뀌어야 함(리셋/재설정 필요)
	if (PawnData)
	{
		return;
	}

	PawnData = InPawnData;

	// 공부 포인트:
	// 만약 클라이언트도 PawnData가 필요하다면,
	// PawnData를 Replicated로 선언하고
	// 여기서 세팅된 값이 클라로 복제되게 해야 함.
}

void AMosesPlayerState::ServerSetSelectedCharacterId(int32 InId)
{
	if (!HasAuthority())
	{
		return;
	}
	SelectedCharacterId = InId;
}

void AMosesPlayerState::ServerSetReady(bool bInReady)
{
	if (!HasAuthority())
	{
		return;
	}
	bReady = bInReady;
}

FString AMosesPlayerState::MakeDebugString() const
{
	return FString::Printf(
		TEXT("PS=%p PlayerId=%d PersistentId=%s DebugName=%s SelChar=%d Ready=%d PlayerName=%s"),
		this,
		GetPlayerId(),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*DebugName,
		SelectedCharacterId,
		bReady ? 1 : 0,
		*GetPlayerName()
	);
}

void AMosesPlayerState::DOD_PS_Log(const UObject* WorldContext, const TCHAR* Where) const
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ENetMode NM = W ? W->GetNetMode() : NM_Standalone;

	const TCHAR* NMStr =
		(NM == NM_DedicatedServer) ? TEXT("DS") :
		(NM == NM_ListenServer) ? TEXT("LS") :
		(NM == NM_Client) ? TEXT("CL") : TEXT("ST");

	// ✅ 고정 1줄 포맷: grep 판정용
	UE_LOG(LogTemp, Warning, TEXT("[DOD][PS][%s][%s] PS=%p PID=%s Char=%d Ready=%d Name=%s"),
		NMStr,
		Where,
		this,
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphensLower),
		SelectedCharacterId,
		bReady ? 1 : 0,
		*GetPlayerName()
	);
}

void AMosesPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (AMosesPlayerState* Dest = Cast<AMosesPlayerState>(PlayerState))
	{
		Dest->PersistentId = PersistentId;
		Dest->DebugName = DebugName;
		Dest->SelectedCharacterId = SelectedCharacterId;
		Dest->bReady = bReady;
		// PawnData는 경험치 로딩 흐름에서 다시 잡힐 수 있으니 정책에 맞게 선택
	}
}

void AMosesPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (AMosesPlayerState* Src = Cast<AMosesPlayerState>(PlayerState))
	{
		PersistentId = Src->PersistentId;
		DebugName = Src->DebugName;
		SelectedCharacterId = Src->SelectedCharacterId;
		bReady = Src->bReady;
	}
}

void AMosesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesPlayerState, PersistentId);
	DOREPLIFETIME(AMosesPlayerState, DebugName);
	DOREPLIFETIME(AMosesPlayerState, SelectedCharacterId);
	DOREPLIFETIME(AMosesPlayerState, bReady);
}
