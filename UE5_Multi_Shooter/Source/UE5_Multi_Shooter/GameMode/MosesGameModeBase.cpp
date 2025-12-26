// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesGameModeBase.h"

#include "MosesGameState.h"
#include "MosesExperienceDefinition.h"
#include "MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "UE5_Multi_Shooter/Character/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/MosesPawnData.h"

#include "Engine/NetConnection.h"
#include "Kismet/GameplayStatics.h"

static FString GetConnAddr(APlayerController* PC)
{
	if (!PC) return TEXT("PC=None");
	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		return Conn->LowLevelGetRemoteAddress(true); // ip:port
	}
	return TEXT("Conn=None");
}

AMosesGameModeBase::AMosesGameModeBase()
{
	// 서버 기본 클래스 셋업(프로젝트 규칙의 베이스)
	GameStateClass = AMosesGameState::StaticClass();
	PlayerControllerClass = AMosesPlayerController::StaticClass();
	PlayerStateClass = AMosesPlayerState::StaticClass();
	DefaultPawnClass = AMosesCharacter::StaticClass();
}

void AMosesGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.f,
			FColor::Red,
			FString::Printf(TEXT("[GM][InitGame] Map=%s Options=%s"), *MapName, *Options)
		);
	}

	UE_LOG(LogMosesExp, Warning, TEXT("[GM][InitGame] Map=%s Options=%s NetMode=%d World=%s"),
		*MapName, *Options, (int32)GetNetMode(), *GetNameSafe(GetWorld()));

	// Experience pick/assign는 다음 틱에서(월드/옵션 세팅 안정화)
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void AMosesGameModeBase::InitGameState()
{
	Super::InitGameState();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.f, FColor::Cyan,
			FString::Printf(TEXT("[GM][InitGameState] GS=%s"), *GetNameSafe(GameState))
		);
	}

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	check(ExperienceManagerComponent);

	UE_LOG(LogMosesExp, Warning, TEXT("[GM][InitGameState] Register OnExperienceLoaded (GS=%s)"),
		*GetNameSafe(GameState));

	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
		FOnMosesExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded)
	);
}

UClass* AMosesGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// PawnData에 PawnClass가 있으면 그걸 사용(모드/직업별 Pawn 확장)
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
	UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] HandleStartingNewPlayer PC=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer), IsExperienceLoaded());

	if (!IsValid(NewPlayer))
	{
		return;
	}

	if (IsExperienceLoaded())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] -> Super::HandleStartingNewPlayer"));
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	// READY 전이면 큐에 넣고, READY에서 Flush로 재시작
	PendingStartPlayers.AddUnique(NewPlayer);
	UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] BLOCKED (waiting READY) -> queued. Pending=%d"),
		PendingStartPlayers.Num());
}

APawn* AMosesGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Pawn 생성 전에 PawnData를 주입하기 위해 Defer Spawn 사용
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;

	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo))
		{
			// PawnExtension에 PawnData를 먼저 주입 → 이후 초기화가 PawnData 기준으로 진행
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
	}

	return nullptr;
}

void AMosesGameModeBase::HandleMatchAssignmentIfNotExpectingOne()
{
	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));

	FPrimaryAssetId ExperienceId;
	const FString MapName = GetWorld() ? GetWorld()->GetMapName() : TEXT("WorldNone");

	// (1) 함수에 들어왔는지부터 화면으로 확인
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.f, FColor::Yellow,
			FString::Printf(TEXT("[EXP][Pick] Enter Map=%s Options=%s"), *MapName, *OptionsString)
		);
	}

	// 1) 옵션으로 Experience 지정 가능: ?Experience=Exp_Lobby (또는 Exp_Match)
	if (UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		ExperienceId = FPrimaryAssetId(ExperienceType, FName(*ExperienceFromOptions));

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Pick] FromOptions Experience=%s (Map=%s)"),
			*ExperienceId.ToString(), *MapName);

		// (2) 옵션에서 뽑았다는 걸 화면으로 확인
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 8.f, FColor::Yellow,
				FString::Printf(TEXT("[EXP][Pick] FromOptions -> %s"), *ExperienceId.ToString())
			);
		}
	}

	// 2) 옵션이 없으면 맵 이름으로 로비/매치 판단해서 기본값 선택
	if (!ExperienceId.IsValid())
	{
		const FString LowerMap = MapName.ToLower();
		const bool bIsLobbyLike = LowerMap.Contains(TEXT("lobby"));
		const FName DefaultExpName = bIsLobbyLike ? FName(TEXT("Exp_Lobby")) : FName(TEXT("Exp_Match"));

		ExperienceId = FPrimaryAssetId(ExperienceType, DefaultExpName);

		UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Pick] Fallback Experience=%s (Map=%s)"),
			*ExperienceId.ToString(), *MapName);

		// (3) Fallback으로 골랐다는 걸 화면으로 확인
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 8.f, FColor::Yellow,
				FString::Printf(TEXT("[EXP][Pick] Fallback -> %s"), *ExperienceId.ToString())
			);
		}
	}

	// (4) 최종 선택 결과 화면 확인
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.f, FColor::Yellow,
			FString::Printf(TEXT("[EXP][Pick] Chosen=%s (Valid=%d)"), *ExperienceId.ToString(), ExperienceId.IsValid() ? 1 : 0)
		);
	}

	check(ExperienceId.IsValid());

	// (5) Assign으로 넘기기 직전 화면 확인(여기까지 오면 Pick은 통과)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.f, FColor::Orange,
			FString::Printf(TEXT("[EXP][Assign] Call -> %s"), *ExperienceId.ToString())
		);
	}

	OnMatchAssignmentGiven(ExperienceId);
}


bool AMosesGameModeBase::IsExperienceLoaded() const
{
	// Experience 로딩 완료 여부(스폰 게이트 판단)
	check(GameState);

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	check(ExperienceManagerComponent);

	return ExperienceManagerComponent->IsExperienceLoaded();
}

void AMosesGameModeBase::OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.f,
			FColor::Green,
			FString::Printf(TEXT("[EXP][READY] %s"), *GetNameSafe(CurrentExperience))
		);
	}

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][READY] Loaded Exp=%s"), *GetNameSafe(CurrentExperience));

	// READY에서 SpawnGate 해제(NextTick Flush)
	OnExperienceReady_SpawnGateRelease();
}

void AMosesGameModeBase::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId)
{
	// 화면 디버그: 서버가 어떤 Experience를 선택했는지 보여주기
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.f, FColor::Orange,
			FString::Printf(TEXT("[EXP][Assign] %s"), *ExperienceId.ToString())
		);
	}

	// 로그: "지금부터 Experience를 서버에 세팅할 거다"
	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Assign] -> ServerSetCurrentExperience %s"), *ExperienceId.ToString());

	// GameMode는 서버에만 있으니,
	// 서버가 가진 GameState에서 ExperienceManagerComponent를 찾는다
	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	check(ExperienceManagerComponent);

	// 서버 RPC 호출:
	// "이번 매치의 Experience를 이걸로 확정해!"
	ExperienceManagerComponent->ServerSetCurrentExperience(ExperienceId);

	// 로그: 호출 끝
	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][Assign] <- ServerSetCurrentExperience called"));
}

void AMosesGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const FString Addr = GetConnAddr(NewPlayer);
	const FString PSName = (NewPlayer && NewPlayer->PlayerState) ? NewPlayer->PlayerState->GetPlayerName() : TEXT("PS=None");

	UE_LOG(LogMosesSpawn, Warning, TEXT("[NET][PostLogin] PC=%s PSName=%s Addr=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer), *PSName, *Addr, IsExperienceLoaded());

	UNetConnection* Conn = NewPlayer ? NewPlayer->GetNetConnection() : nullptr;
	const FString RemoteAddr = Conn ? Conn->LowLevelGetRemoteAddress(true) : TEXT("None");

	if (NewPlayer && NewPlayer->PlayerState)
	{
		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[SERVER][PlayerState OK] PlayerId=%d | PS=%s | RemoteAddr=%s"),
			NewPlayer->PlayerState->GetPlayerId(),
			*GetNameSafe(NewPlayer->PlayerState),
			*RemoteAddr
		);
	}
	else
	{
		UE_LOG(LogMosesSpawn, Error,
			TEXT("[SERVER][PlayerState MISSING] PC=%s | RemoteAddr=%s"),
			*GetNameSafe(NewPlayer),
			*RemoteAddr
		);
	}
}

void AMosesGameModeBase::RestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] RestartPlayer Controller=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer), IsExperienceLoaded());

	// ✅ READY 전 스폰 절대 금지(혹시 다른 경로에서 호출돼도 막아줌)
	if (!IsExperienceLoaded())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SPAWN] RestartPlayer BLOCKED (not READY)"));
		return;
	}

	Super::RestartPlayer(NewPlayer);

	APawn* PawnNow = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] RestartPlayer DONE Pawn=%s"),
		*GetNameSafe(PawnNow));
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
	AActor* Start = Super::ChoosePlayerStart_Implementation(Player);

	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] ChoosePlayerStart PC=%s -> Start=%s Loc=%s"),
		*GetNameSafe(Player),
		*GetNameSafe(Start),
		Start ? *Start->GetActorLocation().ToString() : TEXT("NONE"));

	return Start;
}

void AMosesGameModeBase::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] FinishRestartPlayer Controller=%s"),
		*GetNameSafe(NewPlayer));

	Super::FinishRestartPlayer(NewPlayer, StartRotation);

	APawn* PawnNow = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] FinishRestartPlayer DONE Pawn=%s"),
		*GetNameSafe(PawnNow));
}

const UMosesPawnData* AMosesGameModeBase::GetPawnDataForController(const AController* InController) const
{
	// PawnData 우선순위: PlayerState > Experience DefaultPawnData
	if (InController)
	{
		if (const AMosesPlayerState* YJPS = InController->GetPlayerState<AMosesPlayerState>())
		{
			if (const UMosesPawnData* PawnData = YJPS->GetPawnData<UMosesPawnData>())
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
		return Experience ? Experience->DefaultPawnData : nullptr;
	}

	return nullptr;
}

// ------------------------------
// SpawnGate Release / Flush
// ------------------------------
void AMosesGameModeBase::OnExperienceReady_SpawnGateRelease()
{
	UE_LOG(LogTemp, Warning, TEXT("[READY] Experience READY -> Release SpawnGate. Pending=%d"),
		PendingStartPlayers.Num());

	// 델리게이트 순서 문제 회피: PS(PawnData) 세팅 등이 끝나게 NextTick에서 Flush
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &AMosesGameModeBase::FlushPendingPlayers);
	}
}

void AMosesGameModeBase::FlushPendingPlayers()
{
	if (bFlushingPendingPlayers)
	{
		return;
	}
	bFlushingPendingPlayers = true;

	const int32 PendingCount = PendingStartPlayers.Num();
	UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] UNBLOCKED -> FlushPendingPlayers Count=%d ExpLoaded=%d"),
		PendingCount, IsExperienceLoaded());

	for (TWeakObjectPtr<APlayerController>& WeakPC : PendingStartPlayers)
	{
		APlayerController* PC = WeakPC.Get();
		if (!IsValid(PC))
		{
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("[READY] -> Super::HandleStartingNewPlayer PC=%s"),
			*GetNameSafe(PC));

		// Super 경로를 타는 게 안전: 내부적으로 RestartPlayer까지 정상 플로우 수행
		Super::HandleStartingNewPlayer_Implementation(PC);
	}

	PendingStartPlayers.Reset();
	bFlushingPendingPlayers = false;
}
