// ============================================================================
// UE5_Multi_Shooter/Lobby/GameMode/MosesLobbyGameMode.cpp  (FULL - REORDERED)
// - ctor → Engine → StartGame/Travel → CharacterSelect → Helpers → Catalog → DevNick
// ============================================================================

#include "MosesLobbyGameMode.h"

#include "UE5_Multi_Shooter/MosesPlayerController.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "UE5_Multi_Shooter/Lobby/GameState/MosesLobbyGameState.h"
#include "UE5_Multi_Shooter/Lobby/UI/CharacterSelect/MSCharacterCatalog.h"
#include "UE5_Multi_Shooter/Experience/MosesExperienceDefinition.h"

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

	// BeginPlay에서도 다시 맞춘다(정책 유지)
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

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] BeginPlay Seamless=%d MatchLevel=%s RootPath=%s"),
		bUseSeamlessTravel ? 1 : 0,
		*MatchLevelName.ToString(),
		*MatchMapRootPath);

	// 런타임 중복 호출 방지 상태 초기화
	bTravelToMatchStarted = false;
}

void AMosesLobbyGameMode::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	UE_LOG(LogMosesExp, Log, TEXT("[LobbyGM][READY] Exp=%s"), *GetNameSafe(CurrentExperience));
}

void AMosesLobbyGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	if (!HasAuthority() || !C)
	{
		return;
	}

	AMosesPlayerController* MPC = Cast<AMosesPlayerController>(C);
	if (!MPC)
	{
		return;
	}

	AMosesPlayerState* PS = MPC->GetPlayerState<AMosesPlayerState>();
	if (!PS)
	{
		return;
	}

	// SSOT: Pid 보장
	PS->EnsurePersistentId_Server();

	// [정책] 로비 진입만으로 로그인 금지
	PS->ServerSetLoggedIn(false);

	// SeamlessTravel로 따라온 플레이어도 여기서 DevNick 보장
	EnsureDevNickname_Server(PS);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGM][GPI] OK PC=%s Pid=%s Nick='%s' LoggedIn=%d"),
		*GetNameSafe(MPC),
		*PS->GetPersistentId().ToString(),
		*PS->GetPlayerNickName(),
		PS->IsLoggedIn() ? 1 : 0);
}

void AMosesLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

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

	// [정책] PostLogin에서도 로그인 금지 유지
	PS->ServerSetLoggedIn(false);

	// PostLogin에서도 DevNick 보장(중복 호출돼도 안전)
	EnsureDevNickname_Server(PS);

	UE_LOG(LogMosesSpawn, Log, TEXT("[LobbyGM] PostLogin OK PC=%s Pid=%s Nick='%s' LoggedIn=%d"),
		*GetNameSafe(MPC),
		*PS->GetPersistentId().ToString(),
		*PS->GetPlayerNickName(),
		PS->IsLoggedIn() ? 1 : 0);
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

	LogTravelCall_Evidence(TEXT("HandleStartMatchRequest"));

	if (bTravelToMatchStarted)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] StartMatch SKIP (AlreadyTraveling)"));
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

	// 여기서부터는 단 1회만 허용
	bTravelToMatchStarted = true;

	const FString MapPath = BuildMatchTravelURL();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s"), *MapPath);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[LobbyGM] ServerTravel FAIL (World NULL)"));
		bTravelToMatchStarted = false;
		return;
	}

	World->ServerTravel(MapPath, /*bAbsolute*/ true);
}

void AMosesLobbyGameMode::TravelToMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	LogTravelCall_Evidence(TEXT("TravelToMatch(Exec)"));

	if (bTravelToMatchStarted)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ExecTravel SKIP (AlreadyTraveling)"));
		return;
	}

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

	if (bTravelToMatchStarted)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravelToMatch SKIP (AlreadyTraveling)"));
		return;
	}
	bTravelToMatchStarted = true;

	bUseSeamlessTravel = bUseSeamlessTravelToMatch;

	const FString MapPath = BuildMatchTravelURL();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] ServerTravel -> %s (Seamless=%d)"),
		*MapPath,
		bUseSeamlessTravel ? 1 : 0);

	World->ServerTravel(MapPath, /*bAbsolute*/ true);
}

FString AMosesLobbyGameMode::BuildMatchTravelURL() const
{
	const FString Root = MatchMapRootPath.IsEmpty() ? TEXT("/Game/Map/") : MatchMapRootPath;
	const FString MapAssetPath = FString::Printf(TEXT("%s%s"), *Root, *MatchLevelName.ToString());

	// Warmup Experience로 진입 (정책)
	return FString::Printf(TEXT("%s?listen?Experience=Exp_Match_Warmup"), *MapAssetPath);
}

void AMosesLobbyGameMode::LogTravelCall_Evidence(const TCHAR* From) const
{
	const UWorld* World = GetWorld();

	UE_LOG(LogMosesSpawn, Error,
		TEXT("[LobbyGM][TRAVEL_CALL] From=%s Time=%.3f World=%s NetMode=%d Started=%d Seamless=%d"),
		From ? From : TEXT("None"),
		World ? World->GetTimeSeconds() : -1.f,
		*GetNameSafe(World),
		World ? (int32)World->GetNetMode() : -1,
		bTravelToMatchStarted ? 1 : 0,
		bUseSeamlessTravelToMatch ? 1 : 0);
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

APawn* AMosesLobbyGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM] SpawnDefaultPawnFor BLOCKED PC=%s Start=%s"),
		*GetNameSafe(NewPlayer),
		*GetNameSafe(StartSpot));

	return nullptr;
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

// =========================================================
// Dev nickname
// =========================================================

void AMosesLobbyGameMode::EnsureDevNickname_Server(AMosesPlayerState* PS) const
{
	check(HasAuthority());

	if (!PS)
	{
		return;
	}

	const FString CurrentNick = PS->GetPlayerNickName().TrimStartAndEnd();
	if (!CurrentNick.IsEmpty())
	{
		return;
	}

	const int32 Id = PS->GetPlayerId();
	const FString FinalNick = FString::Printf(TEXT("Dev_Moses_%d"), Id);

	PS->ServerSetPlayerNickName(FinalNick);
	PS->ServerSetLoggedIn(false);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[LobbyGM][DevNick] Applied Nick='%s' (LoggedIn=false) PS=%s"),
		*FinalNick,
		*GetNameSafe(PS));
}
