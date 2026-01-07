#include "MosesLobbyGameMode.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Engine/World.h"

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	// =========================================================
	// 개발자 주석: LobbyGameMode의 역할
	// =========================================================
	// - GameMode는 "서버에만 존재하는 심판"이다.
	// - 클라가 버튼을 눌렀더라도:
	//   1) 요청의 진입점은 PlayerController(Server RPC)
	//   2) 최종 판정/권한/규칙은 GameMode
	//   3) 모두가 공유하는 상태는 GameState/PlayerState(Replication)
	//
	// - 즉, 이 클래스는 "서버 권한 API 엔트리"들을 모아두는 곳이다.
	//
	// - 실무 팁:
	//   GameMode는 클라에 없으므로,
	//   클라에서 직접 호출하려는 습관이 생기면 설계가 꼬인다.
	//   클라이언트는 버튼을 누르고,
	//	 PlayerController를 통해 서버에게 요청을 보내고,
	//	 서버는 GameMode에서 규칙을 판단한 뒤,
	//	 결과를 GameState나 PlayerState에 기록하고,
	//	 그 변경 사실이 Replication으로 모든 클라이언트에게 전달되면,
	//	 각 클라이언트는 RepNotify나 델리게이트를 통해
	//	 자기 화면(UI·카메라·연출)만 반응한다.


	// =========================================================
	// 클래스 정책 (로비용 기본 클래스 세팅)
	// =========================================================
	PlayerControllerClass = AMosesPlayerController::StaticClass();
	PlayerStateClass = AMosesPlayerState::StaticClass();
	GameStateClass = AMosesLobbyGameState::StaticClass();

	// 개발자 주석:
	// - 로비는 Pawn이 꼭 필요하지 않다.
	// - (카메라는 레벨 배치된 CameraActor로 SetViewTargetWithBlend로 전환)
	DefaultPawnClass = nullptr;

	// 개발자 주석:
	// - SeamlessTravel 사용 여부는 정책 값(bUseSeamlessTravelToMatch)로 관리한다.
	// - 에디터에서 값이 바뀌었을 때도 BeginPlay에서 다시 동기화한다(안전).
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;
}

void AMosesLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 개발자 주석:
	// - BeginPlay에서도 다시 동기화.
	// - (에디터/디폴트값/인스턴스 값이 섞일 수 있기 때문에 "최종 실행 직전"에 확정)
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] BeginPlay Seamless=%d TravelURL=%s"),
		bUseSeamlessTravel ? 1 : 0,
		*MatchMapTravelURL
	);
}

void AMosesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// =========================================================
	// 개발자 주석: PostLogin에서 하는 이유
	// =========================================================
	// - PostLogin은 서버에서 "PC/PS가 확정되는 대표 타이밍"이다.
	// - 여기서 PersistentId / LoggedIn 같은 기본 신분값을 확정해두면,
	//   이후 로직(방 입장/레디/캐릭터 선택/대화모드)에서
	//   "Pid가 없어서 터짐", "로그인 안 됐다고 Reject" 같은 사건이 급감한다.
	//
	// - Phase0에서는 로그인 UI/DB를 생략하므로
	//   "서버에서 자동 로그인 확정"을 여기서 해준다.

	AMosesPlayerController* MPC = Cast<AMosesPlayerController>(NewPlayer);
	if (!MPC)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] PostLogin REJECT (Not MosesPC) PC=%s"), *GetNameSafe(NewPlayer));
		return;
	}

	AMosesPlayerState* PS = MPC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] PostLogin REJECT (NoPlayerState) PC=%s"), *GetNameSafe(MPC));
		return;
	}

	// 1) 서버 고유 식별자 보장
	PS->EnsurePersistentId_Server();

	// 2) 자동 로그인 확정 (임시 정책)
	PS->SetLoggedIn_Server(true);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGM] PostLogin OK PC=%s Pid=%s LoggedIn=%d"),
		*GetNameSafe(MPC),
		*PS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens),
		PS->IsLoggedIn() ? 1 : 0
	);
}

// =========================================================
// Start Game (서버 최종 판정 + Travel)
// =========================================================

void AMosesLobbyGameMode::HandleStartGameRequest(AMosesPlayerController* RequestPC)
{
	// 개발자 주석:
	// - "StartGame"은 서버에서만 실행되어야 한다.
	// - 클라 버튼 → PC Server RPC → 여기(서버) → 최종 검증 → ServerTravel
	if (!HasAuthority())
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("HandleStartGameRequest"));
	if (!LGS)
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(RequestPC, TEXT("HandleStartGameRequest"));
	if (!PS)
	{
		return;
	}

	// ✅ 서버 최종 검증: "Host인지" + "AllReady인지"
	// 개발자 주석:
	// - 진짜 정답은 GameState(전광판)에서 관리 중이라면,
	//   "시작 가능 판정"도 GameState에서 하는 게 좋다(단일 진실).
	FString FailReason;
	const bool bOk = LGS->Server_CanStartGame(PS, FailReason);

	if (!bOk)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame REJECT Reason=%s HostPid=%s"),
			*FailReason,
			*PS->GetPersistentId().ToString()
		);
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartGame ACCEPT HostPid=%s -> Travel"),
		*PS->GetPersistentId().ToString()
	);

	DoServerTravelToMatch();
}

void AMosesLobbyGameMode::TravelToMatch()
{
	// 개발자 주석:
	// - Debug/Exec용(혹은 개발 중 강제 이동)
	// - 최종 이동 함수는 항상 DoServerTravelToMatch()로 단일화한다.
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch (Debug/Exec)"));
	DoServerTravelToMatch();
}

// =========================================================
// Character Select (서버 전용 확정 → PlayerState 단일진실)
// =========================================================

void AMosesLobbyGameMode::HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId)
{
	// 개발자 주석:
	// - 캐릭터 선택은 "서버가 확정한 결과"가 모두에게 공유되어야 한다.
	// - 따라서 서버에서 검증하고 PlayerState(단일진실)에 값을 박는다.
	if (!HasAuthority())
	{
		return;
	}

	if (!RequestPC)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (Null PC)"));
		return;
	}

	AMosesPlayerState* PS = RequestPC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (No PS) PC=%s"), *GetNameSafe(RequestPC));
		return;
	}

	// Phase0 최소 검증
	if (CharacterId.IsNone())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (None CharacterId) Pid=%s"),
			*PS->GetPersistentId().ToString()
		);
		return;
	}

	// 카탈로그에서 유효한 ID인지 해석
	const int32 SelectedId = ResolveCharacterId(CharacterId);
	if (SelectedId < 0)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (Invalid CharacterId=%s) Pid=%s"),
			*CharacterId.ToString(),
			*PS->GetPersistentId().ToString()
		);
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[CharSel][SV][GM] ACCEPT Pid=%s CharacterId=%s -> SelectedId=%d"),
		*PS->GetPersistentId().ToString(),
		*CharacterId.ToString(),
		SelectedId
	);

	// ✅ 서버가 정답 확정(단일진실: PlayerState)
	// 개발자 주석:
	// - PlayerState는 복제되므로 다른 클라/late join도 동일 선택 상태를 받는다.
	PS->ServerSetSelectedCharacterId(SelectedId);

	// (선택) DoD 로그(네 프로젝트에서 이미 쓰는 방식)
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterSelectCharacter"));
}

// =========================================================
// Helper: Checked Getters (로그 포함)
// =========================================================

AMosesLobbyGameState* AMosesLobbyGameMode::GetLobbyGameStateChecked_Log(const TCHAR* Caller) const
{
	// 개발자 주석:
	// - 서버 쪽 흐름 디버깅에서 "GS가 null"은 흔한 원인.
	// - Caller를 남기면 어떤 함수에서 실패했는지 바로 추적 가능.
	UWorld* World = GetWorld();
	AMosesLobbyGameState* LGS = World ? World->GetGameState<AMosesLobbyGameState>() : nullptr;

	if (!LGS)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[%s] FAIL (NoLobbyGameState)"), Caller);
	}

	return LGS;
}

AMosesPlayerState* AMosesLobbyGameMode::GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const
{
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[%s] FAIL (NullPC)"), Caller);
		return nullptr;
	}

	AMosesPlayerState* PS = PC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[%s] FAIL (NoPlayerState) PC=%s"), Caller, *GetNameSafe(PC));
		return nullptr;
	}

	return PS;
}

// =========================================================
// Travel (서버 이동 단일 함수)
// =========================================================

void AMosesLobbyGameMode::DoServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 개발자 주석:
	// - SeamlessTravel 정책 재동기화(에디터에서 값 바뀌어도 안전)
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s (Seamless=%d)"),
		*MatchMapTravelURL,
		bUseSeamlessTravel ? 1 : 0
	);

	// 개발자 주석:
	// - bAbsolute=false: 현재 URL에 기반한 travel (일반적)
	World->ServerTravel(MatchMapTravelURL, /*bAbsolute*/false);
}

// =========================================================
// Character Catalog Resolve
// =========================================================

int32 AMosesLobbyGameMode::ResolveCharacterId(const FName CharacterId) const
{
	// 개발자 주석:
	// - CharacterCatalog는 "서버에서만 신뢰 가능한 목록"이어야 한다.
	// - 클라가 넘긴 CharacterId가 조작되더라도,
	//   서버는 Catalog를 기준으로 유효성 판단 후 인덱스로 확정한다.
	if (!CharacterCatalog)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharSel][SV][GM] FAIL (CharacterCatalog NULL)"));
		return -1;
	}

	const TArray<FMSCharacterEntry>& Entries = CharacterCatalog->GetEntries();

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].CharacterId == CharacterId)
		{
			// Phase0 정책:
			// - 카탈로그 인덱스를 SelectedCharacterId로 사용한다.
			// - 나중에 캐릭터가 많아지면 별도 StableId(예: DataTable RowName)로 바꿀 수도 있다.
			return i;
		}
	}

	return -1;
}

// =========================================
// Dialogue Phase Enter/Exit 권한 체크 
// =========================================

bool AMosesLobbyGameMode::CanEnterLobbyDialogue(APlayerController* RequestPC) const
{
	// 개발자 주석:
	// - DAY6 최소판정.
	// - 나중에 여기 확장:
	//   - 방장만 가능
	//   - 현재 Phase가 Lobby일 때만
	//   - AllReady / 최소 인원 만족
	return RequestPC != nullptr;
}

bool AMosesLobbyGameMode::CanExitLobbyDialogue(APlayerController* RequestPC) const
{
	return RequestPC != nullptr;
}

// =========================================================
// (선택) StartGame 연결용 래퍼 (현재는 미사용일 수 있음)
// =========================================================

void AMosesLobbyGameMode::HandleStartGame(AMosesPlayerController* RequestPC)
{
	// 개발자 주석:
	// - 네 프로젝트에서 StartGame 처리 함수명이 바뀌거나
	//   외부에서 "HandleStartGame" 이름으로 통일 호출하고 싶을 때 쓰는 래퍼.
	// - 지금 네 실제 구현은 HandleStartGameRequest()에 있으니,
	//   이 함수를 쓰려면 아래에서 그쪽을 호출하도록 연결하면 된다.

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] HandleStartGame ENTER PC=%s"), *GetNameSafe(RequestPC));

	// ✅ 현재 네 구현에 맞춰서 그대로 연결
	HandleStartGameRequest(RequestPC);
}

// =========================================================
// Dialogue Request handlers (PC->Server RPC가 여기로 위임)
// =========================================================

void AMosesLobbyGameMode::HandleRequestEnterLobbyDialogue(APlayerController* RequestPC)
{
	// 개발자 주석:
	// - PC(Server RPC) -> GM(검증/판정) -> GS(ServerEnter...)
	// - 이 흐름을 유지하면:
	//   - PC에 상태가 쌓이지 않는다(요청 게이트 역할만 유지)
	//   - GM이 최종 규칙을 가진다
	//   - GS는 단일 진실(복제 전광판)로 남는다
	if (!HasAuthority() || !CanEnterLobbyDialogue(RequestPC))
	{
		return;
	}

	AMosesLobbyGameState* GS = GetGameState<AMosesLobbyGameState>();
	if (!GS)
	{
		return;
	}

	GS->ServerEnterLobbyDialogue();
}

void AMosesLobbyGameMode::HandleRequestExitLobbyDialogue(APlayerController* RequestPC)
{
	if (!HasAuthority() || !CanExitLobbyDialogue(RequestPC))
	{
		return;
	}

	AMosesLobbyGameState* GS = GetGameState<AMosesLobbyGameState>();
	if (!GS)
	{
		return;
	}

	GS->ServerExitLobbyDialogue();
}

// =========================================================
// Dialogue progression API (서버가 진행시키는 뼈대)
// =========================================================

void AMosesLobbyGameMode::ServerAdvanceLine(int32 NextLineIndex, float Duration, bool bNPCSpeaking)
{
	// 개발자 주석:
	// - "대사 진행"은 서버가 타이밍/순서를 확정해야 한다.
	// - 클라가 임의로 NextLineIndex를 바꾸면 싱크가 무너진다.
	if (!HasAuthority())
	{
		return;
	}

	if (AMosesLobbyGameState* GS = GetGameState<AMosesLobbyGameState>())
	{
		GS->ServerAdvanceLine(NextLineIndex, Duration, bNPCSpeaking);
	}
}

void AMosesLobbyGameMode::ServerSetSubState(int32 NewSubStateAsInt, float RemainingTime, bool bNPCSpeaking)
{
	if (!HasAuthority())
	{
		return;
	}

	// 개발자 주석:
	// - 외부에서 int로 들어올 수 있으니 서버에서 Clamp.
	// - 0(None), 1(Listening), 2(Speaking)
	const int32 Clamped = FMath::Clamp(NewSubStateAsInt, 0, 2);
	const EDialogueSubState NewSub = static_cast<EDialogueSubState>(Clamped);

	if (AMosesLobbyGameState* GS = GetGameState<AMosesLobbyGameState>())
	{
		GS->ServerSetSubState(NewSub, RemainingTime, bNPCSpeaking);
	}
}

void AMosesLobbyGameMode::HandleDialogueAdvanceLineRequest(APlayerController* RequestPC)
{
	if (!HasAuthority() || !RequestPC)
	{
		return;
	}

	AMosesLobbyGameState* GS = GetGameState<AMosesLobbyGameState>();
	if (!GS)
	{
		return;
	}

	// (선택) 권한 게이트: 호스트만 가능 등 정책 넣고 싶으면 여기서
	GS->ServerAdvanceLine();
}

void AMosesLobbyGameMode::HandleDialogueSetFlowStateRequest(APlayerController* RequestPC, EDialogueFlowState NewState)
{
	if (!HasAuthority() || !RequestPC)
	{
		return;
	}

	AMosesLobbyGameState* GS = GetGameState<AMosesLobbyGameState>();
	if (!GS)
	{
		return;
	}

	GS->ServerSetFlowState(NewState);
}
