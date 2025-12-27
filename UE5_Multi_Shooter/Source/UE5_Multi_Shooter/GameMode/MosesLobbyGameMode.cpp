#include "MosesLobbyGameMode.h"
#include "MosesGameState.h"

#include "UE5_Multi_Shooter/GameMode/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

/*
 * =========================================================
 *  AMosesLobbyGameMode
 *  - 로비 전용 규칙(GameMode)
 *  - Start 버튼 요청 검증 + Match로 ServerTravel 트리거
 *  - SeamlessTravel 왕복 시 PlayerState 유지/재생성 관찰 로그
 * =========================================================
 */

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	/*
	 * [공부 포인트] bUseSeamlessTravel
	 * - ServerTravel 시 “SeamlessTravel”을 쓰면
	 *   * PlayerController / PlayerState 등을 최대한 유지하며(정책에 따라)
	 *   * Transition Map을 거쳐 매끄럽게 맵 전환이 된다.
	 * - 로비↔매치 왕복 디버깅 시 “유지되는 것 / 재생성되는 것”을 관찰하기 좋다.
	 */
	bUseSeamlessTravel = true;

	/*
	 * [공부 포인트] GameStateClass 교체
	 * - GameMode는 서버에만 존재(권한/룰), GameState는 서버+클라에 복제(상태 공유).
	 * - 로비에선 “방 목록/룸 상태” 같은 걸 복제하고 싶으니
	 *   로비 전용 GameState(AMosesLobbyGameState)를 쓰는 게 정석.
	 */
	GameStateClass = AMosesLobbyGameState::StaticClass();
}

/** (Server) Start 버튼 요청 처리: 룰 검증 + Travel */
void AMosesLobbyGameMode::HandleStartGameRequest(AMosesPlayerController* RequestPC)
{
	/*
	 * [공부 포인트] GameMode는 서버 전용이지만,
	 * 코드가 어디서 호출될지(실수로 클라에서 호출 등) 방어적으로 체크해두면 디버깅이 쉬워진다.
	 */
	if (!RequestPC || !HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetGameState<AMosesLobbyGameState>();
	AMosesPlayerState* PS = RequestPC->GetPlayerState<AMosesPlayerState>();

	if (!LGS || !PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (NoGS/NoPS)"));
		return;
	}

	// 1) 방에 들어가 있어야 함
	// [공부 포인트] “요청자”의 PS가 현재 어떤 룸에 속하는지로 권한/검증을 한다.
	if (!PS->RoomId.IsValid())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (NoRoom) PC=%s"), *GetNameSafe(RequestPC));
		return;
	}

	// 2) 호스트만 Start 가능
	// [공부 포인트] 룸의 “결정 권한”은 호스트에게만 부여(클라 조작 방지)
	if (!PS->bIsRoomHost)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (NotHost) PC=%s"), *GetNameSafe(RequestPC));
		return;
	}

	// 3) 룸 정원 충족 + 올레디 체크
	// [공부 포인트] “Start 가능한 조건”은 GameState(복제 상태) 쪽에서 판단하는 게 자연스럽다.
	// - GameMode는 룰 트리거(Travel), GameState는 상태(몇 명, Ready 여부, 룸 구성 등)
	FString RejectReason;
	if (!LGS->Server_CanStartMatch(PS->RoomId, RejectReason))
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT (%s) Room=%s"),
			*RejectReason, *PS->RoomId.ToString());
		return;
	}

	// 4) 통과 → Travel
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame ACCEPT Room=%s -> TravelToMatch"), *PS->RoomId.ToString());
	TravelToMatch();
}

// 기존
void AMosesLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	/*
	 * [공부 포인트] InitGame
	 * - 맵 로드 시작 시점에 Options를 조작할 수 있다.
	 * - URL 옵션("?Key=Value")은 Experience 선택 같은 초기 설정에 흔히 사용.
	 */
	FString FinalOptions = Options;

	// Experience 옵션이 없으면 로비 Experience를 기본으로 강제
	if (UGameplayStatics::HasOption(Options, TEXT("Experience")) == false)
	{
		FinalOptions += TEXT("?Experience=Exp_Lobby");
	}

	Super::InitGame(MapName, FinalOptions, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[LobbyGM][InitGame] Map=%s Options=%s"), *MapName, *FinalOptions);
}

void AMosesLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	/*
	 * [공부 포인트] BeginPlay
	 * - 월드가 “플레이 상태”로 들어간 이후 찍히는 로그.
	 * - SeamlessTravel 왕복 이후에도 다시 BeginPlay가 호출되는지/언제 호출되는지 확인용으로 좋다.
	 */
	UE_LOG(LogMosesExp, Warning, TEXT("[DOD][Travel] LobbyGM Seamless=%d"), bUseSeamlessTravel ? 1 : 0);

	// ✅ 복귀 후 로비에서도 자동으로 찍히게 (PS 유지/재생성 관찰)
	DumpAllDODPlayerStates(TEXT("LobbyGM:BeginPlay_AfterReturn"));

	// (기존) Raw PlayerState 포인터/이름 위주 덤프
	DumpPlayerStates(TEXT("[LobbyGM][BeginPlay]"));
}

/** 복귀 후 정책 적용/증명 로그용 */
void AMosesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	/*
	 * [공부 포인트] PostLogin
	 * - 서버에서 “플레이어가 완전히 로그인 처리된 직후” 호출.
	 * - 로비 복귀(SeamlessTravel 후)에도 호출될 수 있으니
	 *   “복귀 정책(Ready 초기화 등)”을 적용하기에 좋은 포인트다.
	 */
	ApplyReturnPolicy(NewPlayer);
}

void AMosesLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	/*
	 * [공부 포인트] Logout
	 * - 플레이어가 연결 종료/나가기 시 서버에서 호출.
	 * - 로비 룸을 운영할 때 “유령 인원”이 남지 않도록
	 *   방에서 제거하는 처리가 거의 필수.
	 */
	AMosesLobbyGameState* LGS = GetGameState<AMosesLobbyGameState>();
	APlayerController* PC = Cast<APlayerController>(Exiting);
	AMosesPlayerState* PS = PC ? PC->GetPlayerState<AMosesPlayerState>() : nullptr;

	if (LGS && PS)
	{
		LGS->Server_LeaveRoom(PS);
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] Logout -> LeaveRoom PS=%s"), *GetNameSafe(PS));
	}
}

/** Travel 직전/후 PS 유지 증명용 로그 포인트 */
void AMosesLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	/*
	 * [공부 포인트] HandleSeamlessTravelPlayer
	 * - SeamlessTravel 과정에서 “플레이어 컨트롤러/플레이어스테이트” 인계가 일어나는 타이밍.
	 * - 유지되는지, 교체되는지, 어느 시점에 무엇이 존재하는지 로그로 확인하기 좋다.
	 */
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
		{
			PS->DOD_PS_Log(this, TEXT("GM:HandleSeamlessTravelPlayer"));
		}
	}
}

void AMosesLobbyGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	/*
	 * [공부 포인트] GetSeamlessTravelActorList
	 * - SeamlessTravel 시 “어떤 Actor를 같이 들고갈지” 목록을 결정하는 포인트.
	 * - 기본 구현은 PlayerController/PlayerState 등 핵심은 처리해주지만,
	 *   커스텀으로 특정 Actor(예: 매치 설정 매니저)를 유지하고 싶으면 여기에서 추가한다.
	 */
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] GetSeamlessTravelActorList bToTransition=%d ActorListNum=%d"),
		bToTransition, ActorList.Num());
}

/** 서버 콘솔에서 매치로 이동 */
void AMosesLobbyGameMode::TravelToMatch()
{
	/*
	 * [공부 포인트] ServerTravel은 서버만 호출해야 한다.
	 * - 클라에서 호출하면 아무 일도 안 되거나, 경고/오동작의 원인이 된다.
	 */
	if (!CanDoServerTravel())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch BLOCKED"));
		return;
	}

	// ✅ Travel 직전 DoD Dump: “전환 직전의 PS 상태” 증거 확보
	DumpAllDODPlayerStates(TEXT("Lobby:BeforeTravelToMatch"));
	DumpPlayerStates(TEXT("[LobbyGM][BeforeTravelToMatch]"));

	const FString URL = GetMatchMapURL();
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s"), *URL);

	/*
	 * [공부 포인트] ServerTravel(URL, bAbsolute)
	 * - bAbsolute=false면 현재 옵션 유지/상대 경로 처리 등 엔진 규칙에 따름.
	 * - Dedicated Server가 이미 떠 있으면 일반적으로 ?listen 불필요.
	 */
	GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
}

/* =========================
 * Private helpers (정책/로그)
 * ========================= */

FString AMosesLobbyGameMode::GetMatchMapURL() const
{
	/*
	 * [공부 포인트] URL 정책을 “한 곳에서 고정”
	 * - 나중에 옵션(게임모드, 경험치, 매치 규칙, 로비 복귀 URL 등)이 늘어나면
	 *   여기만 수정하면 되게끔 구조화.
	 */
	return TEXT("/Game/Map/L_Match");
}

void AMosesLobbyGameMode::DumpPlayerStates(const TCHAR* Prefix) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("%s GameState=None"), Prefix);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("%s PlayerArray Num=%d"), Prefix, GS->PlayerArray.Num());

	/*
	 * [공부 포인트] GameStateBase::PlayerArray
	 * - 서버/클라 모두가 “현재 월드에 존재하는 PlayerState 목록”을 공유한다.
	 * - SeamlessTravel 왕복 시 PlayerState 포인터/PlayerId/이름이 유지되는지 체크 가능.
	 */
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

void AMosesLobbyGameMode::DumpAllDODPlayerStates(const TCHAR* Where) const
{
	const AGameStateBase* GS = GameState;
	if (!GS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] GameState=None"), Where);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] ---- DumpAllPS Begin ----"), Where);

	/*
	 * [공부 포인트] “고정 포맷 로그”의 장점
	 * - 로그를 grep/필터링해서 “전환 전/후/복귀 후”를 비교할 수 있다.
	 * - PlayerState 유지/재생성, Ready/Host/RoomId 등 핵심 필드를 한 줄에 찍으면 강력함.
	 */
	for (APlayerState* RawPS : GS->PlayerArray)
	{
		if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(RawPS))
		{
			PS->DOD_PS_Log(this, Where);
		}
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][PS][DS][%s] ---- DumpAllPS End ----"), Where);
}

bool AMosesLobbyGameMode::CanDoServerTravel() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[DOD][Travel] REJECT (NoWorld)"));
		return false;
	}

	const ENetMode NM = World->GetNetMode();
	const bool bIsClient = (NM == NM_Client);

	/*
	 * [공부 포인트] NetMode vs HasAuthority
	 * - NetMode(NM_Client)는 “이 인스턴스가 클라이언트 프로세스인가?”에 가깝고,
	 * - HasAuthority는 “이 액터가 서버 권한을 가지는가?”에 가깝다.
	 * - 둘 다 체크해두면 ‘왜 막혔는지’ 로그 원인 파악이 쉬워진다.
	 */
	if (bIsClient)
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

void AMosesLobbyGameMode::ApplyReturnPolicy(APlayerController* PC)
{
	if (!PC) return;

	if (AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>())
	{
		/*
		 * [공부 포인트] “복귀 정책”
		 * - SeamlessTravel 왕복 후에도 PS가 유지되면 값이 그대로 남아있을 수 있다.
		 * - 로비로 돌아오면 Ready를 false로 초기화하는 게 일반적인 UX/규칙.
		 */
		PS->bReady = false;

		// SelectedCharacterId 유지 정책(편의). 리셋 원하면 해제
		// PS->SelectedCharacterId = 0;

		// 고정 포맷 로그로 정책 적용 증거 남기기
		PS->DOD_PS_Log(this, TEXT("Lobby:Policy_ResetReadyOnReturn"));
	}
}

int32 AMosesLobbyGameMode::GetPlayerCount() const
{
	/*
	 * [공부 포인트] “전체 로비 인원수”
	 * - 일단은 PlayerArray 크기로 반환.
	 * - “룸 별 인원”을 세려면 AMosesLobbyGameState의 룸 데이터에서 카운트하는 게 맞다.
	 */
	const AGameStateBase* GS = GameState;
	return GS ? GS->PlayerArray.Num() : 0;
}

int32 AMosesLobbyGameMode::GetReadyCount() const
{
	/*
	 * [공부 포인트] “Ready 인원수”
	 * - 여기서는 PlayerArray 전체에서 AMosesPlayerState를 캐스팅해 bReady를 센다.
	 * - 룸 단위 ReadyCount가 필요하면 “같은 RoomId만” 필터링해서 세면 된다.
	 */
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
