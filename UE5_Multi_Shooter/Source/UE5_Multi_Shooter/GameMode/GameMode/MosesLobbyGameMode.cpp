#include "MosesLobbyGameMode.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/UI/CharacterSelect/MSCharacterCatalog.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// =========================================================
// Engine
// =========================================================

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	// =========================================================
	// 클래스 정책 (로비용 기본 클래스 세팅)
	// =========================================================
	PlayerControllerClass = AMosesPlayerController::StaticClass();
	PlayerStateClass = AMosesPlayerState::StaticClass();
	GameStateClass = AMosesLobbyGameState::StaticClass();

	// 로비는 Pawn이 꼭 필요하지 않다.
	DefaultPawnClass = nullptr;

	// SeamlessTravel 정책은 별도 변수로 관리하고,
	// 실제 적용은 BeginPlay에서도 재동기화(에디터 변경 안전).
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;
}

void AMosesLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// 로비 맵은 반드시 Exp_Lobby가 선택되게 옵션을 강제한다.
	FString FinalOptions = Options;
	if (!UGameplayStatics::HasOption(Options, TEXT("Experience")))
	{
		FinalOptions += TEXT("?Experience=Exp_Lobby");
	}

	Super::InitGame(MapName, FinalOptions, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[LobbyGM][InitGame] Map=%s Options=%s"), *MapName, *FinalOptions);
}

void AMosesLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 에디터/디폴트값/인스턴스 값 혼재 가능 → 최종 실행 직전 확정
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] BeginPlay Seamless=%d MatchLevel=%s"),
		bUseSeamlessTravel ? 1 : 0,
		*MatchLevelName.ToString()
	);
}

void AMosesLobbyGameMode::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	UE_LOG(LogMosesExp, Log, TEXT("[LobbyGM][READY] Exp=%s"), *GetNameSafe(CurrentExperience));
}

void AMosesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

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

	// 2) ✅ 개별 로그인 정책: 여기서 자동 로그인 처리 금지
	PS->SetLoggedIn_Server(false);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGM] PostLogin OK PC=%s Pid=%s LoggedIn=%d"),
		*GetNameSafe(MPC),
		*PS->GetPersistentId().ToString(), // 너 함수명에 맞춰
		PS->IsLoggedIn() ? 1 : 0);

	HandleStartMatchRequest(PS);
}

// =========================================================
// Start Game (서버 최종 판정 + Travel)
// =========================================================

void AMosesLobbyGameMode::HandleStartGameRequest(AMosesPlayerController* RequestPC)
{
	if (!HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = GetMosesPlayerStateChecked_Log(RequestPC, TEXT("HandleStartGameRequest"));
	if (!PS)
	{
		return;
	}

	// ✅ 최종 엔트리(PS 기반)로 통일
	HandleStartMatchRequest(PS);
}

void AMosesLobbyGameMode::HandleStartMatchRequest(AMosesPlayerState* HostPS)
{
	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[TEST] StartMatchRequest Host=%s"),
		*GetNameSafe(HostPS));

	if (!HasAuthority() || !HostPS)
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("HandleStartMatchRequest"));
	if (!LGS)
	{
		return;
	}

	// ✅ 2명(정원) 다 찼고 + (호스트 제외) 전원 Ready인지 서버에서 최종 검증
	FString FailReason;
	if (!LGS->Server_CanStartGame(HostPS, FailReason))
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartMatch REJECT Reason=%s"), *FailReason);
		return;
	}

	// ✅ 매치맵으로 이동
	const FString MapPath = TEXT("/Game/Map/MatchLevel?listen");
	// - listen: ListenServer일 때 필요 (DedicatedServer면 없어도 됨)
	// - Experience 옵션 등 필요하면 "?Experience=Exp_Match" 같이 붙이면 됨

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s"), *MapPath);

	GetWorld()->ServerTravel(MapPath, /*bAbsolute*/ true);
}

void AMosesLobbyGameMode::TravelToMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] TravelToMatch (Debug/Exec)"));
	ServerTravelToMatch();
}

void AMosesLobbyGameMode::ServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// SeamlessTravel 정책 재동기화(에디터 변경 안전)
	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	// ✅ 핵심: Match로 갈 때도 Experience를 옵션으로 명시
	const FString MapPath = FString::Printf(TEXT("/Game/Maps/%s?Experience=Exp_Match"),
		*MatchLevelName.ToString()
	);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s (Seamless=%d)"),
		*MapPath,
		bUseSeamlessTravel ? 1 : 0
	);

	World->ServerTravel(MapPath, /*bAbsolute*/ true);
}

// =========================================================
// Character Select (서버 전용 확정 → PlayerState 단일진실)
// =========================================================

void AMosesLobbyGameMode::HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId)
{
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

	PS->ServerSetSelectedCharacterId(SelectedId);
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterSelectCharacter"));
}

// =========================================================
// Helper: Checked Getters (로그 포함)
// =========================================================

AMosesLobbyGameState* AMosesLobbyGameMode::GetLobbyGameStateChecked_Log(const TCHAR* Caller) const
{
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
// Character Catalog Resolve
// =========================================================

int32 AMosesLobbyGameMode::ResolveCharacterId(const FName CharacterId) const
{
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
	return RequestPC != nullptr;
}

bool AMosesLobbyGameMode::CanExitLobbyDialogue(APlayerController* RequestPC) const
{
	return RequestPC != nullptr;
}

// =========================================================
// Dialogue Request handlers (PC->Server RPC가 여기로 위임)
// =========================================================

void AMosesLobbyGameMode::HandleRequestEnterLobbyDialogue(APlayerController* RequestPC)
{
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
// Dialogue progression API
// =========================================================

void AMosesLobbyGameMode::ServerAdvanceLine(int32 NextLineIndex, float Duration, bool bNPCSpeaking)
{
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

bool AMosesLobbyGameMode::Gate_AcceptClientCommand(APlayerController* RequestPC, uint16 ClientCommandSeq, FString& OutRejectReason)
{
	if (!RequestPC)
	{
		OutRejectReason = TEXT("no_pc");
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	uint16& LastSeq = LastAcceptedClientSeqByPC.FindOrAdd(RequestPC);
	float& LastTime = LastAcceptedTimeByPC.FindOrAdd(RequestPC);

	if (ClientCommandSeq <= LastSeq)
	{
		OutRejectReason = FString::Printf(TEXT("duplicate_or_old (seq=%u last=%u)"), ClientCommandSeq, LastSeq);
		return false;
	}

	if ((Now - LastTime) < CommandCooldownSec)
	{
		OutRejectReason = FString::Printf(TEXT("cooldown (dt=%.3f < %.3f)"), (Now - LastTime), CommandCooldownSec);
		return false;
	}

	LastSeq = ClientCommandSeq;
	LastTime = Now;
	return true;
}

bool AMosesLobbyGameMode::Gate_IsCommandAllowedInCurrentFlow(EDialogueCommandType Type, const FDialogueNetState& NetState, FString& OutRejectReason) const
{
	switch (Type)
	{
	case EDialogueCommandType::Pause:
		if (NetState.FlowState != EDialogueFlowState::Speaking)
		{
			OutRejectReason = TEXT("pause_not_allowed (not speaking)");
			return false;
		}
		return true;

	case EDialogueCommandType::Resume:
		if (NetState.FlowState != EDialogueFlowState::Paused)
		{
			OutRejectReason = TEXT("resume_not_allowed (not paused)");
			return false;
		}
		return true;

	case EDialogueCommandType::Exit:
		return true;

	case EDialogueCommandType::Skip:
		if (NetState.FlowState == EDialogueFlowState::WaitingInput)
		{
			OutRejectReason = TEXT("skip_not_allowed (waiting input)");
			return false;
		}
		return true;

	case EDialogueCommandType::Repeat:
	case EDialogueCommandType::Restart:
		return true;

	default:
		OutRejectReason = TEXT("unknown_type");
		return false;
	}
}

void AMosesLobbyGameMode::HandleSubmitDialogueCommand(APlayerController* RequestPC, EDialogueCommandType Type, uint16 ClientCommandSeq)
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

	FString Reject;
	if (!Gate_AcceptClientCommand(RequestPC, ClientCommandSeq, Reject))
	{
		UE_LOG(LogTemp, Log, TEXT("[Cmd] Reject (%s) type=%d seq=%u"), *Reject, (int32)Type, ClientCommandSeq);
		return;
	}

	const FDialogueNetState& NetState = GS->GetDialogueNetState();

	FString FlowReject;
	if (!Gate_IsCommandAllowedInCurrentFlow(Type, NetState, FlowReject))
	{
		UE_LOG(LogTemp, Log, TEXT("[Cmd] Reject (%s) type=%d seq=%u"), *FlowReject, (int32)Type, ClientCommandSeq);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Cmd][S] Accept type=%d seq=%u"), (int32)Type, ClientCommandSeq);

	switch (Type)
	{
	case EDialogueCommandType::Pause:
		GS->ServerSetFlowState(EDialogueFlowState::Paused);
		break;

	case EDialogueCommandType::Resume:
		GS->ServerSetFlowState(EDialogueFlowState::Speaking);
		break;

	case EDialogueCommandType::Exit:
		GS->ServerExitLobbyDialogue();
		break;

	case EDialogueCommandType::Skip:
		GS->ServerAdvanceLine();
		break;

	case EDialogueCommandType::Repeat:
		GS->ServerRepeatCurrentLine();
		break;

	case EDialogueCommandType::Restart:
		GS->ServerRestartDialogue();
		break;

	default:
		break;
	}
}
