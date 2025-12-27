#include "MosesPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

#include "UE5_Multi_Shooter/Character/MosesPawnData.h"
#include "UE5_Multi_Shooter/GameMode/MosesExperienceManagerComponent.h"
#include "UE5_Multi_Shooter/GameMode/MosesGameModeBase.h"

AMosesPlayerState::AMosesPlayerState()
{
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

void AMosesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesPlayerState, PersistentId);
	DOREPLIFETIME(AMosesPlayerState, DebugName);
	DOREPLIFETIME(AMosesPlayerState, SelectedCharacterId);
	DOREPLIFETIME(AMosesPlayerState, bReady);
	DOREPLIFETIME(AMosesPlayerState, RoomId);
	DOREPLIFETIME(AMosesPlayerState, bIsRoomHost);
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

void AMosesPlayerState::EnsurePersistentId_ServerOnly()
{
	// 서버에서만 PersistentId를 생성/보장
	if (HasAuthority() && !PersistentId.IsValid())
	{
		PersistentId = FGuid::NewGuid();
	}
}

// ----------------------------------------------------
// Experience 로딩 완료 시점에 PlayerState가 반응하는 함수
// ----------------------------------------------------
// ⚠️ 중요 개념 요약
// - 이 함수는 PlayerState에 있으므로 "서버/클라 모두에서 호출될 수 있음"
// - 하지만, 내부 로직은 GetAuthGameMode()를 사용하므로
//   "실제로 의미 있는 동작은 서버에서만 수행"된다.
//
// 역할 요약:
// - 서버: 이 플레이어가 어떤 PawnData(직업/캐릭터 세트)를 사용할지 결정
// - 클라: 이 함수는 호출될 수 있으나, 실제 로직은 실행되지 않음
//
void AMosesPlayerState::OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience)
{
	// ------------------------------------------------
	// 서버 전용 분기
	// ------------------------------------------------
	// GameMode는 서버에만 존재하는 객체이다.
	// 따라서 GetAuthGameMode()가 nullptr이 아니면
	// "지금 이 코드는 서버에서 실행 중"임을 의미한다.
	if (AMosesGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AMosesGameModeBase>())
	{
		// PlayerState의 Owner는 서버 기준으로
		// 해당 플레이어의 Controller(PlayerController)이다.
		AController* OwnerController = Cast<AController>(GetOwner());

		// GameMode 정책에 따라
		// "이 Controller가 어떤 PawnData를 써야 하는지"를 결정한다.
		// - Experience 종류
		// - 팀
		// - 로비에서 선택한 캐릭터
		// 등의 정보를 종합해 서버가 단일 진실(Source of Truth)을 만든다.
		const UMosesPawnData* NewPawnData =
			GameMode->GetPawnDataForController(OwnerController);

		// 서버에서 PawnData를 못 찾으면
		// 이후 스폰/Ability/Input 초기화가 전부 망가질 수 있으므로
		// 즉시 크래시로 문제를 드러내는 것이 낫다.
		check(NewPawnData);

		// PlayerState에 PawnData를 세팅한다.
		// 이 값은:
		// - 서버에서는 스폰 정책의 기준이 되고
		// - Replication을 통해 클라이언트로 전달된다.
		SetPawnData(NewPawnData);

		UE_LOG(LogTemp, Warning,
			TEXT("[PS][PawnData][SERVER] SetPawnData PS=%s PC=%s PawnData=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerController),
			*GetNameSafe(NewPawnData)
		);
	}
	else
	{
		// ------------------------------------------------
		// 클라이언트 분기 (실제 로직 없음)
		// ------------------------------------------------
		// 클라이언트에서는:
		// - GameMode가 존재하지 않기 때문에
		// - 항상 이 else 경로로 들어온다.
		//
		// 즉, 클라이언트는 PawnData를 "결정"하지 않는다.
		// 클라이언트는 서버가 PlayerState에 세팅한 PawnData를
		// Replication(OnRep_PawnData)을 통해 전달받아 사용해야 한다.
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
	if (HasAuthority() == false)
	{
		return;
	}

	SelectedCharacterId = InId;
}

void AMosesPlayerState::ServerSetReady(bool bInReady)
{
	if (HasAuthority() == false)
	{
		return;
	}

	bReady = bInReady;
}

void AMosesPlayerState::ServerSetRoom(const FGuid& InRoomId, bool bInIsHost)
{
	// PlayerState는 서버가 “단일 진실(Source of Truth)”로 들고 있고,
	// RoomId / Host 같은 로비 상태는 여기서 복제해주면 UI가 자동 갱신된다.
	if (HasAuthority() == false)
	{
		return;
	}

	RoomId = InRoomId;
	bIsRoomHost = bInIsHost;
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

	// 고정 1줄 포맷: grep 판정용
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
