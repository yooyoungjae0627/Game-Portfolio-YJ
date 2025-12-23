// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesGameModeBase.h"

//#include "MosesGameState.h"
//#include "MosesExperienceDefinition.h"
//#include "MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
//#include "Project_YJ_A/Player/YJPlayerController.h"
//#include "Project_YJ_A/Player/YJPlayerState.h"
//#include "Project_YJ_A/Character/YJCharacter.h"
#include "UE5_Multi_Shooter/Character/MosesPawnData.h"
//#include "Project_YJ_A/Character/YJPawnExtensionComponent.h"

#include "Kismet/GameplayStatics.h"

AMosesGameModeBase::AMosesGameModeBase()
{
	// 서버 기본 클래스 셋업(프로젝트 규칙의 베이스)
	//GameStateClass = AYJGameState::StaticClass();
	//PlayerControllerClass = AYJPlayerController::StaticClass();
	//PlayerStateClass = AYJPlayerState::StaticClass();
	//DefaultPawnClass = AYJCharacter::StaticClass();
}

void AMosesGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// 맵 로드 직후(OptionsString F 가능)
	Super::InitGame(MapName, Options, ErrorMessage);

	// 이 시점엔 GameState/컴포넌트 준비가 덜 될 수 있어 NextTick에서 Experience 결정
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void AMosesGameModeBase::InitGameState()
{
	Super::InitGameState();

	// ExperienceManager의 "로드 완료" 이벤트를 받아 스폰을 재개한다.
	//UYJExperienceManagerComponent* ExperienceManagerComponent =
	//	GameState->FindComponentByClass<UYJExperienceManagerComponent>();
	//check(ExperienceManagerComponent);

	//ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
	//	FOnYJExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded)
	//);
}

UClass* AMosesGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// PawnData에 PawnClass가 있으면 그걸 사용(모드/직업별 Pawn 확장)
	if (const UMosesPawnData* PawnData = GetPawnDataForController(InController))
	{
		//if (PawnData->PawnClass)
		//{
		//	return PawnData->PawnClass;
		//}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AMosesGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	//UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] HandleStartingNewPlayer PC=%s ExpLoaded=%d"),
	//	*GetNameSafe(NewPlayer), IsExperienceLoaded());

	//if (IsExperienceLoaded())
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] -> Super::HandleStartingNewPlayer"));
	//	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	//}
	//else
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("[SpawnGate] BLOCKED (waiting READY)"));
	//}
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
			//if (UMosesPawnExtensionComponent* PawnExtComp = UMosesPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
			//{
			//	if (const UMosesPawnData* PawnData = GetPawnDataForController(NewPlayer))
			//	{
			//		PawnExtComp->SetPawnData(PawnData);
			//	}
			//}

			SpawnedPawn->FinishSpawning(SpawnTransform);
			return SpawnedPawn;
		}
	}

	return nullptr;
}

void AMosesGameModeBase::HandleMatchAssignmentIfNotExpectingOne()
{
	FPrimaryAssetId ExperienceId;

	// 1) URL 옵션 우선 (?Experience=Exp_Lobby 같은 식)
	if (ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")) == false)
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));

		// ✅ PrimaryAssetType은 AssetManager 설정의 타입명과 동일해야 한다.
		// Project Settings > AssetManager 에서 "YJExperienceDefinition" 타입으로 등록했으면 그걸 사용.
		ExperienceId = FPrimaryAssetId(
			FPrimaryAssetType(TEXT("MosesExperienceDefinition")),
			FName(*ExperienceFromOptions)
		);

		UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] OptionsString chose Experience=%s -> %s"),
			*ExperienceFromOptions, *ExperienceId.ToString());
	}

	// 2) 옵션이 없으면 맵 이름 기반 폴백(로비/매치 분리 DoD용)
	if (!ExperienceId.IsValid())
	{
		// MapName은 /Game/Maps/Lobby 같은 경로로 들어올 수 있음
		const FString LowerMap = GetWorld()->GetMapName().ToLower();

		const bool bIsLobbyLike = LowerMap.Contains(TEXT("lobby"));

		const FName DefaultExpName = bIsLobbyLike ? FName(TEXT("Exp_Lobby")) : FName(TEXT("Exp_Match"));

		ExperienceId = FPrimaryAssetId(
			FPrimaryAssetType(TEXT("MosesExperienceDefinition")),
			DefaultExpName
		);

		UE_LOG(LogTemp, Log, TEXT("[Moses][Exp] Fallback chose Experience=%s (Map=%s)"),
			*DefaultExpName.ToString(), *GetWorld()->GetMapName());
	}

	check(ExperienceId.IsValid());
	OnMatchAssignmentGiven(ExperienceId);
}

//bool AMosesGameModeBase::IsExperienceLoaded() const
//{
//	// Experience 로딩 완료 여부(스폰 게이트 판단)
//	//check(GameState);
//
//	//UMosesExperienceManagerComponent* ExperienceManagerComponent =
//	//	GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
//	//check(ExperienceManagerComponent);
//
//	return ExperienceManagerComponent->IsExperienceLoaded();
//}

//void AMosesGameModeBase::OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience)
//{
//	UE_LOG(LogTemp, Warning, TEXT("[READY] OnExperienceLoaded Exp=%s"),
//		*GetNameSafe(CurrentExperience));
//
//	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
//	{
//		APlayerController* PC = Cast<APlayerController>(*It);
//
//		const bool bHasPawn = (PC && PC->GetPawn());
//		const bool bCanRestart = (PC && PlayerCanRestart(PC));
//
//		UE_LOG(LogTemp, Warning, TEXT("[READY] PC=%s HasPawn=%d CanRestart=%d"),
//			*GetNameSafe(PC), bHasPawn, bCanRestart);
//
//		if (PC && !bHasPawn && bCanRestart)
//		{
//			UE_LOG(LogTemp, Warning, TEXT("[READY] -> RestartPlayer(%s)"), *GetNameSafe(PC));
//			RestartPlayer(PC);
//			UE_LOG(LogTemp, Warning, TEXT("[READY] <- RestartPlayer PawnNow=%s"),
//				*GetNameSafe(PC->GetPawn()));
//		}
//	}
//}


void AMosesGameModeBase::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId)
{
	UE_LOG(LogTemp, Log, TEXT("[GameMode] Experience 결정: %s"), *ExperienceId.ToString());

	// ExperienceManager에게 선택된 Experience 로딩을 지시
	//check(ExperienceId.IsValid());

	//UMosesExperienceManagerComponent* ExperienceManagerComponent =
	//	GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	//check(ExperienceManagerComponent);

	//ExperienceManagerComponent->ServerSetCurrentExperience(ExperienceId);
}

void AMosesGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] PostLogin PC=%s HasPawn=%d"),
		*GetNameSafe(NewPlayer), NewPlayer && NewPlayer->GetPawn() ? 1 : 0);
}

void AMosesGameModeBase::RestartPlayer(AController* NewPlayer)
{
	//UE_LOG(LogTemp, Warning, TEXT("[SPAWN] RestartPlayer Controller=%s ExpLoaded=%d"),
	//	*GetNameSafe(NewPlayer), IsExperienceLoaded());

	//Super::RestartPlayer(NewPlayer);

	//APawn* PawnNow = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	//UE_LOG(LogTemp, Warning, TEXT("[SPAWN] RestartPlayer DONE Pawn=%s"),
	//	*GetNameSafe(PawnNow));
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
	//if (InController)
	//{
	//	if (const AMosesPlayerState* YJPS = InController->GetPlayerState<AMosesPlayerState>())
	//	{
	//		if (const UMosesPawnData* PawnData = YJPS->GetPawnData<UMosesPawnData>())
	//		{
	//			return PawnData;
	//		}
	//	}
	//}

	//check(GameState);

	//UMosesExperienceManagerComponent* ExperienceManagerComponent =
	//	GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	//check(ExperienceManagerComponent);

	//if (ExperienceManagerComponent->IsExperienceLoaded())
	//{
	//	const UMosesExperienceDefinition* Experience = ExperienceManagerComponent->GetCurrentExperienceChecked();
	//	return Experience ? Experience->DefaultPawnData : nullptr;
	//}

	return nullptr;
}



