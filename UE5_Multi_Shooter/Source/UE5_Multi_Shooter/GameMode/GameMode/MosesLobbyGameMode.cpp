#include "MosesLobbyGameMode.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

/*
 * AMosesLobbyGameMode
 *
 * 책임:
 * - 로비 서버 규칙(권한/검증) 담당
 * - Start 요청 검증 후 Match로 ServerTravel 트리거
 * - SeamlessTravel 왕복 시 PS 유지/재생성 여부를 로그로 증명(DOD 로그 포함)
 *
 * 주의:
 * - GameMode는 서버에만 존재한다. (클라 UI/표현 로직 절대 금지)
 * - Travel 정책(URL/옵션)은 반드시 GetMatchMapURL() 한 곳에서만 수정한다.
 * - 로비 복귀 정책(Ready 초기화 등)은 ApplyReturnPolicy()에만 둔다.
 */

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	// 로비↔매치 왕복에서 PS/PC 인계 동작을 관찰하기 위해 SeamlessTravel 사용
	// (비활성화하면 PS 재생성 패턴이 바뀌므로 DoD 비교 로그가 깨질 수 있음)
	bUseSeamlessTravel = true;

	// 로비 상태(룸 목록/준비 상태 등) 복제를 위해 로비 전용 GameState 사용
	GameStateClass = AMosesLobbyGameState::StaticClass();
}

/**
 * Start 버튼 요청 처리 (서버 전용)
 * 흐름:
 * 1) 서버/요청자 유효성 체크
 * 2) 요청자 PS 기반으로 Room 소속/Host 권한 검증
 * 3) LGS(Server_CanStartMatch)로 룸 상태(정원/Ready 등) 최종 검증
 * 4) 통과 시 ServerTravel 수행
 *
 * 보안/치트 방지:
 * - 클라에서 위조 요청이 오더라도 서버에서만 판단/실행한다.
 */
void AMosesLobbyGameMode::HandleStartGameRequest(AMosesPlayerController* RequestPC)
{
	// 서버 전용 방어 + 입력 유효성
	if (!RequestPC || !HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetGameState<AMosesLobbyGameState>();
	AMosesPlayerState* PS = RequestPC->GetPlayerState<AMosesPlayerState>();

	// 필수 레퍼런스 누락 시 거부(비정상 상태)
	if (!LGS || !PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (NoGS/NoPS)"));
		return;
	}

	// [검증1] 룸 소속 여부: 방 밖에서 Start 요청 불가
	if (!PS->RoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (NoRoom) PC=%s"), *GetNameSafe(RequestPC));
		return;
	}

	// [검증2] Host 권한: 호스트 외 Start 불가(권한 단일화)
	if (!PS->bIsRoomHost)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (NotHost) PC=%s"), *GetNameSafe(RequestPC));
		return;
	}

	// [검증3] 룸 상태 기반 최종 Start 가능 여부 (정원/Ready/기타 룰)
	// - 상태 소유는 LGS, 룰 트리거(Travel)는 GM
	FString RejectReason;
	if (!LGS->Server_CanStartMatch(PS->RoomId, RejectReason))
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (%s) Room=%s"),
			*RejectReason, *PS->RoomId.ToString());
		return;
	}

	// 조건 통과 → 매치 이동
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame ACCEPT Room=%s -> TravelToMatch"), *PS->RoomId.ToString());
	TravelToMatch();
}

/**
 * 로비 맵 로드 시작 시점 옵션 확정
 * - Experience 옵션이 없으면 로비 Experience를 강제 주입
 * - (주의) 이 구간에서만 Experience 기본값을 넣고, 다른 곳에서 중복 세팅하지 말 것
 */
void AMosesLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	FString FinalOptions = Options;

	// 로비 기본 Experience 강제
	if (UGameplayStatics::HasOption(Options, TEXT("Experience")) == false)
	{
		FinalOptions += TEXT("?Experience=Exp_Lobby");
	}

	Super::InitGame(MapName, FinalOptions, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[LobbyGM][InitGame] Map=%s Options=%s"), *MapName, *FinalOptions);
}

/**
 * 로비 월드 진입 완료 시점
 * - SeamlessTravel 복귀 후에도 호출될 수 있으므로 "복귀 직후 상태" 스냅샷을 남긴다.
 * - DOD 로그는 전환 전/후 비교를 위해 고정 포맷 유지
 */
void AMosesLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Travel/복귀 디버그 기준 로그
	UE_LOG(LogMosesExp, Warning, TEXT("[DOD][Travel] LobbyGM Seamless=%d"), bUseSeamlessTravel ? 1 : 0);

	// 복귀 직후에도 자동으로 찍히게: PS 유지/재생성, Ready/Host/RoomId 값 확인
	DumpAllDODPlayerStates(TEXT("LobbyGM:BeginPlay_AfterReturn"));

	// Raw 포인터/ID/이름 비교용(저수준 증거)
	DumpPlayerStates(TEXT("[LobbyGM][BeginPlay]"));
}

/**
 * 서버에서 플레이어 로그인 완료 직후 호출
 * - SeamlessTravel 복귀 경로에서도 호출될 수 있음
 * - 로비 복귀 정책(Ready 초기화 등)은 반드시 여기서 일관되게 적용
 */
void AMosesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 로비 복귀/재접속 공통 정책 적용 지점
	ApplyReturnPolicy(NewPlayer);
}

/**
 * 서버에서 플레이어 이탈 처리
 * - Disconnect/강종 포함
 * - 룸 상태 정리의 마지막 안전망(유령 인원 방지)
 */
void AMosesLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// 서버 전용
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetGameState<AMosesLobbyGameState>();
	APlayerController* PC = Cast<APlayerController>(Exiting);
	AMosesPlayerState* PS = PC ? PC->GetPlayerState<AMosesPlayerState>() : nullptr;

	// 룸에서 제거(서버 authoritative)
	if (LGS && PS)
	{
		LGS->Server_LeaveRoom(PS);
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] Logout -> LeaveRoom PS=%s"), *GetNameSafe(PS));
	}
}

/**
 * SeamlessTravel 중 플레이어 인계 시점
 * - PS가 유지되는지/교체되는지 관측하는 핵심 로그 포인트
 * - 여기 로그가 바뀌면 Travel 정책 변경의 신호로 봐야 함
 */
void AMosesLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
		{
			PS->DOD_PS_Log(this, TEXT("GM:HandleSeamlessTravelPlayer"));
		}
	}
}

/**
 * SeamlessTravel 시 같이 가져갈 Actor 목록 결정
 * - 기본 처리 외에 "유지해야 하는 서버 Actor"가 생기면 여기에서 추가
 * - 현재는 관측/디버그 목적 로그만 남김
 */
void AMosesLobbyGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] GetSeamlessTravelActorList bToTransition=%d ActorListNum=%d"),
		bToTransition, ActorList.Num());
}

/**
 * 서버 콘솔/디버그에서 매치 이동 트리거
 * - 실제 Start 흐름은 HandleStartGameRequest를 사용
 * - 여기서는 "여건이 되면 바로 Travel"만 수행
 */
void AMosesLobbyGameMode::TravelToMatch()
{
	// ServerTravel 방어(클라/권한/월드 누락 등)
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch BLOCKED"));
		return;
	}

	// 전환 직전 상태 스냅샷(DOD)
	DumpAllDODPlayerStates(TEXT("Lobby:BeforeTravelToMatch"));
	DumpPlayerStates(TEXT("[LobbyGM][BeforeTravelToMatch]"));

	const FString URL = GetMatchMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s"), *URL);

	// bAbsolute=false 유지(옵션 처리 정책이 바뀌면 전환 로그도 같이 재검증 필요)
	GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
}

/* =========================
 * Private helpers (정책/디버그)
 * ========================= */

 /**
  * Match 맵 URL 정책 단일화
  * - 매치 옵션(Experience/룰/리턴 URL 등)이 추가되면 여기만 수정
  * - 다른 함수에서 URL을 하드코딩하지 말 것
  */
FString AMosesLobbyGameMode::GetMatchMapURL() const
{
	return TEXT("/Game/Map/L_Match");
}

/**
 * Raw PlayerState 덤프(저수준 증거)
 * - 포인터/ID/이름 기준으로 "유지 vs 재생성"을 단순 비교할 때 사용
 */
void AMosesLobbyGameMode::DumpPlayerStates(const TCHAR* Prefix) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("%s GameState=None"), Prefix);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("%s PlayerArray Num=%d"), Prefix, GS->PlayerArray.Num());

	for (APlayerState* PS : GS->PlayerArray)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("%s PS=%p Name=%s PlayerId=%d PlayerName=%s"),
			Prefix,
			PS,
			*GetNameSafe(PS),
			PS ? PS->GetPlayerId() : -1,
			PS ? *PS->GetPlayerName() : TEXT("None"));
	}
}

/**
 * DOD 고정 포맷 PlayerState 덤프
 * - grep/필터 기준이 되는 핵심 로그
 * - Travel 전/후/복귀 후 비교에서 "필드 값 변화"를 확인하는 목적
 */
void AMosesLobbyGameMode::DumpAllDODPlayerStates(const TCHAR* Where) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] GameState=None"), Where);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] ---- DumpAllPS Begin ----"), Where);

	for (APlayerState* RawPS : GS->PlayerArray)
	{
		if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(RawPS))
		{
			PS->DOD_PS_Log(this, Where);
		}
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] ---- DumpAllPS End ----"), Where);
}

/**
 * ServerTravel 가능 여부 체크
 * - Travel 호출 지점을 늘리더라도 "여기만 믿고" 공통 방어로 사용
 * - 실패 사유는 로그로 남겨서 원인 추적 가능하게 유지
 */
bool AMosesLobbyGameMode::CanDoServerTravel() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return false;
	}

	const ENetMode NM = World->GetNetMode();
	if (NM == NM_Client)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (Client)"));
		return false;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoAuthority)"));
		return false;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] ACCEPT (Server)"));
	return true;
}

/**
 * 로비 복귀 정책
 * - SeamlessTravel로 PS가 유지되면 Ready 같은 값이 남아있을 수 있음
 * - 로비 UX/룰 기준으로 복귀 시 Ready는 항상 false로 초기화(현재 정책)
 * - 정책 변경(예: Ready 유지)이 필요하면 여기만 수정
 */
void AMosesLobbyGameMode::ApplyReturnPolicy(APlayerController* PC)
{
	if (!PC) return;

	if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
	{
		PS->bReady = false;

		// 캐릭터 선택 유지/리셋 정책은 여기서 결정
		// PS->SelectedCharacterId = 0;

		PS->DOD_PS_Log(this, TEXT("Lobby:Policy_ResetReadyOnReturn"));
	}
}

/**
 * 로비 전체 인원(단순)
 * - 룸 단위 인원이 필요하면 LGS 룸 데이터에서 계산하도록 확장
 */
int32 AMosesLobbyGameMode::GetPlayerCount() const
{
	const AGameStateBase* GS = GameState;
	return GS ? GS->PlayerArray.Num() : 0;
}

/**
 * Ready 인원(단순)
 * - 룸 단위 ReadyCount가 필요하면 RoomId로 필터링해서 별도 함수로 분리 권장
 */
int32 AMosesLobbyGameMode::GetReadyCount() const
{
	const AGameStateBase* GS = GameState;
	if (!GS) return 0;

	int32 Count = 0;
	for (APlayerState* RawPS : GS->PlayerArray)
	{
		if (const AMosesPlayerState* PS = Cast<AMosesPlayerState>(RawPS))
		{
			if (PS->bReady)
			{
				++Count;
			}
		}
	}
	return Count;
}

/**
 * Experience READY 이후 확정 지점
 * - Lobby 초기화 1회 보장
 * - ServerPhase = Lobby 확정(순서 흔들림 방지)
 * - 여기서 초기화 순서를 변경하면 로비 부트/입장 타이밍이 달라질 수 있음
 */
void AMosesLobbyGameMode::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	// 서버 전용 보장
	if (!HasAuthority())
	{
		return;
	}

	// Lobby 시스템 초기화(1회)
	if (AMosesLobbyGameState* LGS = GetGameState<AMosesLobbyGameState>())
	{
		LGS->Server_InitLobbyIfNeeded();
	}

	// 서버 Phase = Lobby 확정
	if (AMosesGameState* GS = GetGameState<AMosesGameState>())
	{
		GS->ServerSetPhase(EMosesServerPhase::Lobby);
	}
}
