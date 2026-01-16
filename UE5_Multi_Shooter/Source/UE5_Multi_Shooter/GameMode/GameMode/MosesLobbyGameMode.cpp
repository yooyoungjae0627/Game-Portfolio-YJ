// ============================================================================
// MosesLobbyGameMode.cpp
// ============================================================================

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
// ctor
// =========================================================

AMosesLobbyGameMode::AMosesLobbyGameMode()
{
	PlayerControllerClass = AMosesPlayerController::StaticClass();
	PlayerStateClass = AMosesPlayerState::StaticClass();
	GameStateClass = AMosesLobbyGameState::StaticClass();

	DefaultPawnClass = nullptr;

	bUseSeamlessTravel = bUseSeamlessTravelToMatch;
}

// =========================================================
// Engine
// =========================================================

void AMosesLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
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

	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] BeginPlay Seamless=%d MatchLevel=%s"),
		bUseSeamlessTravel ? 1 : 0,
		*MatchLevelName.ToString());
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

	PS->EnsurePersistentId_Server();

	// 개별 로그인 정책: 여기서 자동 로그인 처리 금지
	PS->SetLoggedIn_Server(false);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGM] PostLogin OK PC=%s Pid=%s LoggedIn=%d"),
		*GetNameSafe(MPC),
		*PS->GetPersistentId().ToString(),
		PS->IsLoggedIn() ? 1 : 0);

	// ⚠️ 원본 코드에 있던 “PostLogin에서 StartMatchRequest 호출”은 테스트 용도로 보이는데
	// 실제 로비에선 의도치 않게 시작 요청이 연쇄될 수 있음.
	// 필요 시 너가 별도의 테스트 플래그로 감싸서 켜는 걸 권장.
	// HandleStartMatchRequest(PS);
}

// =========================================================
// Start Game (서버 최종 판정 + Travel)
// =========================================================

void AMosesLobbyGameMode::HandleStartMatchRequest(AMosesPlayerState* HostPS)
{
	if (!HasAuthority() || !HostPS)
	{
		return;
	}

	AMosesLobbyGameState* LGS = GetLobbyGameStateChecked_Log(TEXT("HandleStartMatchRequest"));
	if (!LGS)
	{
		return;
	}

	FString FailReason;
	if (!LGS->Server_CanStartGame(HostPS, FailReason))
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartMatch REJECT Reason=%s"), *FailReason);
		return;
	}

	// ✅ 매치맵으로 이동
	const FString MapPath = TEXT("/Game/Map/MatchLevel?listen");

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

// =========================================================
// Travel (server single function)
// =========================================================

void AMosesLobbyGameMode::ServerTravelToMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	const FString MapPath = FString::Printf(TEXT("/Game/Maps/%s?Experience=Exp_Match"),
		*MatchLevelName.ToString());

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s (Seamless=%d)"),
		*MapPath,
		bUseSeamlessTravel ? 1 : 0);

	World->ServerTravel(MapPath, /*bAbsolute*/ true);
}

// =========================================================
// Character Select (Server authoritative → PS single truth)
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

	if (CharacterId.IsNone())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (None CharacterId) Pid=%s"),
			*PS->GetPersistentId().ToString());
		return;
	}

	const int32 SelectedId = ResolveCharacterId(CharacterId);
	if (SelectedId < 0)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharSel][SV][GM] REJECT (Invalid CharacterId=%s) Pid=%s"),
			*CharacterId.ToString(),
			*PS->GetPersistentId().ToString());
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[CharSel][SV][GM] ACCEPT Pid=%s CharacterId=%s -> SelectedId=%d"),
		*PS->GetPersistentId().ToString(),
		*CharacterId.ToString(),
		SelectedId);

	PS->ServerSetSelectedCharacterId(SelectedId);
	PS->DOD_PS_Log(this, TEXT("Lobby:AfterSelectCharacter"));
}

// =========================================================
// Helpers (checked getters)
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
// Character catalog resolve
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
