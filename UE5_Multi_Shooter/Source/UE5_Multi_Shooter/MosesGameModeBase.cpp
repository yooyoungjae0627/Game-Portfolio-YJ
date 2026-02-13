// ============================================================================
// UE5_Multi_Shooter/MosesGameModeBase.cpp
// ============================================================================

#include "MosesGameModeBase.h"

#include "UE5_Multi_Shooter/MosesGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"

#include "UE5_Multi_Shooter/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/Experience/MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/Match/Characters/Player/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Match/Characters/Player/Data/MosesPawnData.h"

#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

AMosesGameModeBase::AMosesGameModeBase()
{
	GameStateClass = AMosesGameState::StaticClass();
}

void AMosesGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[GM][InitGame] Map=%s Options=%s NetMode=%d World=%s"),
		*MapName, *Options, static_cast<int32>(GetNetMode()), *GetNameSafe(GetWorld()));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
	}
}

void AMosesGameModeBase::InitGameState()
{
	Super::InitGameState();

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState ? GameState->FindComponentByClass<UMosesExperienceManagerComponent>() : nullptr;

	check(ExperienceManagerComponent);

	UE_LOG(LogMosesExp, Warning, TEXT("[GM][InitGameState] Register OnExperienceLoaded (GS=%s)"),
		*GetNameSafe(GameState));

	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
		FOnMosesExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void AMosesGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const FString Addr = GetConnAddr(NewPlayer);

	const FString PSName =
		(NewPlayer && NewPlayer->PlayerState)
		? NewPlayer->PlayerState->GetPlayerName()
		: TEXT("PS=None");

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[NET][PostLogin] PC=%s PSName=%s Addr=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer),
		*PSName,
		*Addr,
		IsExperienceLoaded() ? 1 : 0);

	UNetConnection* Conn = NewPlayer ? NewPlayer->GetNetConnection() : nullptr;
	const FString RemoteAddr = Conn ? Conn->LowLevelGetRemoteAddress(true) : TEXT("None");

	if (NewPlayer && NewPlayer->PlayerState)
	{
		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[SERVER][PlayerState OK] PlayerId=%d PS=%s RemoteAddr=%s"),
			NewPlayer->PlayerState->GetPlayerId(),
			*GetNameSafe(NewPlayer->PlayerState),
			*RemoteAddr);
	}
	else
	{
		UE_LOG(LogMosesSpawn, Error,
			TEXT("[SERVER][PlayerState MISSING] PC=%s RemoteAddr=%s"),
			*GetNameSafe(NewPlayer),
			*RemoteAddr);
	}
}

UClass* AMosesGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const UMosesPawnData* PawnData = GetPawnDataForController(InController))
	{
		if (PawnData->PawnClass)
		{
			return PawnData->PawnClass;
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AMosesGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[SpawnGate] HandleStartingNewPlayer PC=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer),
		IsExperienceLoaded() ? 1 : 0);

	if (!IsValid(NewPlayer))
	{
		return;
	}

	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	PendingStartPlayers.AddUnique(NewPlayer);

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[SpawnGate] BLOCKED (waiting READY) -> queued. Pending=%d"),
		PendingStartPlayers.Num());
}

APawn* AMosesGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;

	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass || !GetWorld())
	{
		return nullptr;
	}

	APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (!SpawnedPawn)
	{
		return nullptr;
	}

	if (UMosesPawnExtensionComponent* PawnExtComp = UMosesPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
	{
		if (const UMosesPawnData* PawnData = GetPawnDataForController(NewPlayer))
		{
			PawnExtComp->SetPawnData(PawnData);
		}
	}

	SpawnedPawn->FinishSpawning(SpawnTransform);
	return SpawnedPawn;
}

void AMosesGameModeBase::RestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[SPAWN] RestartPlayer Controller=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer), IsExperienceLoaded() ? 1 : 0);

	if (!IsExperienceLoaded())
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[SPAWN] RestartPlayer BLOCKED (not READY)"));
		return;
	}

	Super::RestartPlayer(NewPlayer);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[SPAWN] RestartPlayer DONE Pawn=%s"),
		*GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr));
}

void AMosesGameModeBase::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	APlayerController* PC = Cast<APlayerController>(Exiting);
	const FString Addr = PC ? GetConnAddr(PC) : TEXT("PC=None");

	UE_LOG(LogMosesSpawn, Warning, TEXT("[NET][Logout] Controller=%s PC=%s Pawn=%s PS=%s Addr=%s"),
		*GetNameSafe(Exiting),
		*GetNameSafe(PC),
		*GetNameSafe(PC ? PC->GetPawn() : nullptr),
		*GetNameSafe(PC ? PC->PlayerState : nullptr),
		*Addr);
}

AActor* AMosesGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> MatchStarts;
	MatchStarts.Reserve(8);

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PS = *It;
		if (PS && PS->ActorHasTag(TEXT("Match")))
		{
			MatchStarts.Add(PS);
		}
	}

	if (MatchStarts.Num() <= 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const int32 Index = FMath::RandRange(0, MatchStarts.Num() - 1);
	return MatchStarts[Index];
}

void AMosesGameModeBase::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[SPAWN] FinishRestartPlayer Controller=%s"),
		*GetNameSafe(NewPlayer));

	Super::FinishRestartPlayer(NewPlayer, StartRotation);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[SPAWN] FinishRestartPlayer DONE Pawn=%s"),
		*GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr));
}

void AMosesGameModeBase::HandleMatchAssignmentIfNotExpectingOne()
{
	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));

	FPrimaryAssetId ExperienceId;
	const FString FullMapName = GetWorld() ? GetWorld()->GetMapName() : TEXT("WorldNone");

	if (UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		ExperienceId = FPrimaryAssetId(ExperienceType, FName(*ExperienceFromOptions));

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Pick] FromOptions Experience=%s (Map=%s)"),
			*ExperienceId.ToString(), *FullMapName);
	}

	if (!ExperienceId.IsValid())
	{
		const FString MapShort = FPackageName::GetShortName(FullMapName);

		FString Normalized = MapShort.ToLower();
		Normalized.RemoveFromStart(TEXT("uedpie_0_"));
		Normalized.RemoveFromStart(TEXT("uedpie_1_"));
		Normalized.RemoveFromStart(TEXT("uedpie_2_"));

		FName DefaultExpName = NAME_None;

		if (Normalized == TEXT("startgamelevel"))
		{
			DefaultExpName = TEXT("Exp_Start");
		}
		else if (Normalized == TEXT("lobbylevel"))
		{
			DefaultExpName = TEXT("Exp_Lobby");
		}
		else if (Normalized == TEXT("matchlevel"))
		{
			DefaultExpName = TEXT("Exp_Match_Warmup");
		}
		else
		{
			DefaultExpName = TEXT("Exp_Start");
		}

		ExperienceId = FPrimaryAssetId(ExperienceType, DefaultExpName);

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Pick] Fallback Experience=%s (FullMap=%s Normalized=%s)"),
			*ExperienceId.ToString(), *FullMapName, *Normalized);
	}

	check(ExperienceId.IsValid());
	OnMatchAssignmentGiven(ExperienceId);
}

void AMosesGameModeBase::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId)
{
	if (!bDoD_ExperienceSelectedLogged)
	{
		bDoD_ExperienceSelectedLogged = true;
		UE_LOG(LogMosesExp, Log, TEXT("[EXP] Experience Selected (%s)"), *ExperienceId.ToString());
	}

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Assign] -> ServerSetCurrentExperience %s"), *ExperienceId.ToString());

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState ? GameState->FindComponentByClass<UMosesExperienceManagerComponent>() : nullptr;

	check(ExperienceManagerComponent);

	ExperienceManagerComponent->ServerSetCurrentExperience(ExperienceId);

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Assign] <- ServerSetCurrentExperience called"));
}

bool AMosesGameModeBase::IsExperienceLoaded() const
{
	check(GameState);

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();

	check(ExperienceManagerComponent);
	return ExperienceManagerComponent->IsExperienceLoaded();
}

void AMosesGameModeBase::OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience)
{
	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][READY] Loaded Exp=%s"), *GetNameSafe(CurrentExperience));

	HandleDoD_AfterExperienceReady(CurrentExperience);
	OnExperienceReady_SpawnGateRelease();
}

void AMosesGameModeBase::OnExperienceReady_SpawnGateRelease()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[READY] Experience READY -> Release SpawnGate. Pending=%d"),
		PendingStartPlayers.Num());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FlushPendingPlayers);
	}
}

void AMosesGameModeBase::FlushPendingPlayers()
{
	if (bFlushingPendingPlayers)
	{
		return;
	}

	bFlushingPendingPlayers = true;

	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[SpawnGate] UNBLOCKED -> FlushPendingPlayers Count=%d ExpLoaded=%d"),
		PendingStartPlayers.Num(), IsExperienceLoaded() ? 1 : 0);

	for (TWeakObjectPtr<APlayerController>& WeakPC : PendingStartPlayers)
	{
		APlayerController* PC = WeakPC.Get();
		if (!IsValid(PC))
		{
			continue;
		}

		UE_LOG(LogMosesSpawn, Warning, TEXT("[READY] -> Super::HandleStartingNewPlayer PC=%s"),
			*GetNameSafe(PC));

		Super::HandleStartingNewPlayer_Implementation(PC);
	}

	PendingStartPlayers.Reset();
	bFlushingPendingPlayers = false;
}

const UMosesPawnData* AMosesGameModeBase::GetPawnDataForController(const AController* InController) const
{
	if (InController)
	{
		if (const AMosesPlayerState* MosesPS = InController->GetPlayerState<AMosesPlayerState>())
		{
			if (const UMosesPawnData* PawnData = MosesPS->GetPawnData<UMosesPawnData>())
			{
				return PawnData;
			}
		}
	}

	check(GameState);

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();

	check(ExperienceManagerComponent);

	if (ExperienceManagerComponent->IsExperienceLoaded())
	{
		const UMosesExperienceDefinition* Experience = ExperienceManagerComponent->GetCurrentExperienceChecked();
		return Experience ? Experience->GetDefaultPawnDataLoaded() : nullptr;
	}

	return nullptr;
}

void AMosesGameModeBase::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	(void)CurrentExperience;
}

void AMosesGameModeBase::ServerTravelToLobby()
{
}

FString AMosesGameModeBase::GetConnAddr(APlayerController* PC)
{
	if (!PC)
	{
		return TEXT("PC=None");
	}

	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		return Conn->LowLevelGetRemoteAddress(true);
	}

	return TEXT("Conn=None");
}
