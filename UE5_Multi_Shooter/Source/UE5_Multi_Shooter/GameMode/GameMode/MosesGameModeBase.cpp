// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesGameModeBase.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesGameState.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Character/MosesCharacter.h"
#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h"

#include "Engine/NetConnection.h"
#include "Kismet/GameplayStatics.h"

AMosesGameModeBase::AMosesGameModeBase()
{
	// 서버 기본 클래스 셋업(프로젝트 규칙의 베이스)
}

void AMosesGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogMosesExp, Warning, TEXT("[GM][InitGame] Map=%s Options=%s NetMode=%d World=%s"),
		*MapName, *Options, (int32)GetNetMode(), *GetNameSafe(GetWorld()));

	// Experience pick/assign는 다음 틱에서(월드/옵션 세팅 안정화)
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void AMosesGameModeBase::InitGameState()
{
	Super::InitGameState();

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	
	check(ExperienceManagerComponent);

	UE_LOG(LogMosesExp, Warning, TEXT("[GM][InitGameState] Register OnExperienceLoaded (GS=%s)"),
		*GetNameSafe(GameState));

	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
		FOnMosesExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded)
	);
}

void AMosesGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	// GameModeBase::PostLogin
	// - 서버 전용 함수
	// - 클라이언트가 서버에 성공적으로 접속(Login)한 직후 호출됨
	// - 이 시점부터 해당 PlayerController는 서버에서 "유효한 플레이어"로 취급됨
	Super::PostLogin(NewPlayer);

	// ------------------------------
	// 기본 디버그 정보 수집
	// ------------------------------

	// 접속 주소(IP:Port) 추출 (디버깅용)
	const FString Addr = GetConnAddr(NewPlayer);

	// PlayerState가 이미 생성되었는지 확인
	// PostLogin 시점에서는 정상적이라면 PlayerState는 이미 생성 완료 상태
	const FString PSName =
		(NewPlayer && NewPlayer->PlayerState)
		? NewPlayer->PlayerState->GetPlayerName()
		: TEXT("PS=None");

	// 서버 관점 로그
	// - 어떤 PC가 들어왔는지
	// - PlayerState가 있는지
	// - Experience(게임 규칙/모드)가 이미 로딩 완료 상태인지 확인
	UE_LOG(LogMosesSpawn, Warning,
		TEXT("[NET][PostLogin] PC=%s PSName=%s Addr=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer),
		*PSName,
		*Addr,
		IsExperienceLoaded()
	);

	// ------------------------------
	// 네트워크 연결 정보 확인
	// ------------------------------

	// PlayerController → NetConnection
	// - Dedicated Server 환경에서는 실제 원격 클라이언트의 소켓 연결
	UNetConnection* Conn = NewPlayer ? NewPlayer->GetNetConnection() : nullptr;

	// 원격 클라이언트 IP 주소 (LowLevel 네트워크 주소)
	const FString RemoteAddr =
		Conn ? Conn->LowLevelGetRemoteAddress(true) : TEXT("None");

	// ------------------------------
	// PlayerState 검증
	// ------------------------------

	if (NewPlayer && NewPlayer->PlayerState)
	{
		// 정상 케이스
		// - PlayerState는 PostLogin 이전에 생성됨
		// - PlayerId는 서버에서 유니크하게 할당됨
		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[SERVER][PlayerState OK] PlayerId=%d | PS=%s | RemoteAddr=%s"),
			NewPlayer->PlayerState->GetPlayerId(),
			*GetNameSafe(NewPlayer->PlayerState),
			*RemoteAddr
		);
	}
	else
	{
		// 비정상 케이스 (이론상 거의 발생하면 안 됨)
		// - PlayerState 생성 실패
		// - 커스텀 GameMode / PlayerState 설정 문제 가능성
		UE_LOG(LogMosesSpawn, Error,
			TEXT("[SERVER][PlayerState MISSING] PC=%s | RemoteAddr=%s"),
			*GetNameSafe(NewPlayer),
			*RemoteAddr
		);
	}
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
	// HandleStartingNewPlayer
	// - 서버 전용
	// - PostLogin 이후, "이 플레이어를 실제 게임에 투입해도 되는지" 판단하는 단계
	// - 기본 구현에서는 여기서 Pawn 스폰 / Possess 흐름이 시작됨
	UE_LOG(LogTemp, Warning,
		TEXT("[SpawnGate] HandleStartingNewPlayer PC=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer),
		IsExperienceLoaded()
	);

	// 안전 체크
	if (IsValid(NewPlayer) == false)
	{
		// 비정상 PlayerController면 즉시 중단
		return;
	}

	// ------------------------------
	// Experience 로딩 완료 여부 체크
	// ------------------------------

	if (IsExperienceLoaded())
	{
		// Experience가 READY 상태라면
		// → 엔진 기본 스폰 파이프라인으로 그대로 진행
		// → Super::HandleStartingNewPlayer 안에서:
		//    - RestartPlayer
		//    - Pawn 스폰
		//    - Possess
		//    - BeginPlay 호출
		UE_LOG(LogTemp, Warning,
			TEXT("[SpawnGate] -> Super::HandleStartingNewPlayer")
		);

		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	// ------------------------------
	// Experience 로딩 전(READY 아님)
	// ------------------------------

	// 아직 Experience가 로딩 중이면
	// → Pawn 스폰을 허용하면 안 됨
	//   (Ability, Input, PawnData, GameFeature 미적용 상태 위험)
	//
	// 따라서:
	// - 플레이어를 즉시 스폰하지 않고
	// - 대기 큐(PendingStartPlayers)에 보관
	// - Experience READY 시점에 Flush하여
	//   다시 HandleStartingNewPlayer를 실행
	PendingStartPlayers.AddUnique(NewPlayer);

	UE_LOG(LogTemp, Warning,
		TEXT("[SpawnGate] BLOCKED (waiting READY) -> queued. Pending=%d"),
		PendingStartPlayers.Num()
	);
}

// ------------------------------------------------------------
// Pawn을 실제로 생성하는 단계 (엔진 스폰 파이프라인의 핵심 지점)
// ------------------------------------------------------------
// 호출 위치(엔진 흐름):
//   GameMode::RestartPlayer()
//     → RestartPlayerAtPlayerStart()
//       → RestartPlayerAtTransform()
//         → SpawnDefaultPawnAtTransform()   ★ 여기 ★
//
// 즉:
// - HandleStartingNewPlayer()를 통과한 이후
// - RestartPlayer()가 호출되었을 때
// - "실제로 Pawn을 하나 만들어라"는 시점에 엔진이 이 함수를 호출한다.
//
// 이 함수의 역할 요약:
// - Pawn을 스폰하되
// - Pawn이 완성되기 *전에*
// - PawnData를 먼저 주입해서
// - Pawn이 태어나는 순간부터 "완성된 데이터 상태"를 보장한다.
//
APawn* AMosesGameModeBase::SpawnDefaultPawnAtTransform_Implementation(
	AController* NewPlayer,
	const FTransform& SpawnTransform
)
{
	// --------------------------------------------------------
	// Defer Spawn 설정
	// --------------------------------------------------------
	// bDeferConstruction = true
	// → SpawnActor 시점에는 Pawn의 Construction Script / BeginPlay가 실행되지 않음
	// → 우리가 원하는 데이터를 먼저 주입할 "틈"을 만든다.
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;

	// --------------------------------------------------------
	// 어떤 Pawn 클래스를 스폰할지 결정
	// --------------------------------------------------------
	// 이 함수는 보통:
	// - GameMode 기본 PawnClass
	// - 또는 PawnData / Controller / Team / Experience 기준으로
	//   동적으로 다른 Pawn 클래스를 반환하도록 오버라이드 가능
	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		// ----------------------------------------------------
		// Pawn 인스턴스 생성 (아직 "미완성 상태")
		// ----------------------------------------------------
		if (APawn* SpawnedPawn =
			GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo))
		{
			// ------------------------------------------------
			// ★ 핵심 설계 포인트 ★
			// Pawn이 완성되기 전에 PawnData를 주입한다
			// ------------------------------------------------
			// 이유:
			// - PawnExtensionComponent는 PawnData를 기준으로
			//   Ability, Input, Camera, Attribute 등을 초기화한다.
			// - PawnData 없이 BeginPlay가 호출되면
			//   "빈 캐릭터"로 초기화되는 위험이 있다.
			if (UMosesPawnExtensionComponent* PawnExtComp =
				UMosesPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
			{
				// 서버 기준으로 "이 Controller가 사용할 PawnData"를 가져온다.
				// (PlayerState에 READY 시점에 세팅된 그 PawnData)
				if (const UMosesPawnData* PawnData =
					GetPawnDataForController(NewPlayer))
				{
					// PawnExtension에 PawnData를 먼저 세팅
					// → 아직 BeginPlay 전이므로 안전
					PawnExtComp->SetPawnData(PawnData);
				}
			}

			// ------------------------------------------------
			// Pawn 완성
			// ------------------------------------------------
			// FinishSpawning 호출 시점에서:
			// - Construction Script 실행
			// - Components 초기화
			// - BeginPlay 호출
			//
			// 즉, 이 시점부터 Pawn은
			// "PawnData가 이미 들어있는 상태"로 살아나게 된다.
			SpawnedPawn->FinishSpawning(SpawnTransform);

			return SpawnedPawn;
		}
	}

	// Pawn 생성 실패
	return nullptr;
}

// 이 로직은 GameMode에 있기 때문에
// 서버에서만 실행되고,
// 클라이언트는 Experience 선택 결과를
// GameState나 ExperienceManager를 통해 전달받는다.
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
	if (ExperienceId.IsValid() == false)
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
	//if (GEngine)
	//{
	//	GEngine->AddOnScreenDebugMessage(
	//		-1,
	//		8.f,
	//		FColor::Green,
	//		FString::Printf(TEXT("[EXP][READY] %s"), *GetNameSafe(CurrentExperience))
	//	);
	//}

	UE_LOG(LogMosesExp, Warning, TEXT("[EXP][READY] Loaded Exp=%s"), *GetNameSafe(CurrentExperience));

	// ✅ 핵심: READY 이후에만 Lobby/Phase를 확정하게 한다.
	// - 이 훅은 LobbyGameMode가 override해서 [ROOM]/[PHASE] DoD 로그를 찍는다.
	HandleDoD_AfterExperienceReady(CurrentExperience);

	// READY에서 SpawnGate 해제(NextTick Flush)
	OnExperienceReady_SpawnGateRelease();
}

void AMosesGameModeBase::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId)
{
	// 화면 디버그: 서버가 어떤 Experience를 선택했는지 보여주기
	//if (GEngine)
	//{
	//	GEngine->AddOnScreenDebugMessage(
	//		-1, 8.f, FColor::Orange,
	//		FString::Printf(TEXT("[EXP][Assign] %s"), *ExperienceId.ToString())
	//	);
	//}

	// DoD: Experience Selected는 "서버가 최종 확정"한 순간에 1회만 로그 고정
	// - ServerSetCurrentExperience 중복 방지 로직이 컴포넌트에도 있지만,
	//   DoD 라인은 여기서 1회만 찍어주는 게 가장 명확함.
	if (!bDoD_ExperienceSelectedLogged)
	{
		bDoD_ExperienceSelectedLogged = true;
		UE_LOG(LogMosesExp, Log, TEXT("[EXP] Experience Selected (%s)"), *ExperienceId.ToString());
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

void AMosesGameModeBase::RestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("[SPAWN] RestartPlayer Controller=%s ExpLoaded=%d"),
		*GetNameSafe(NewPlayer), IsExperienceLoaded());

	// READY 전 스폰 절대 금지(혹시 다른 경로에서 호출돼도 막아줌)
	if (IsExperienceLoaded() == false)
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

// ------------------------------------------------------------
// Experience READY 이후, "대기 중이던 플레이어"들을 한꺼번에 시작시키는 함수
// ------------------------------------------------------------
void AMosesGameModeBase::FlushPendingPlayers()
{
	// [재진입 방지]
	// Flush 도중에(예: Super 내부에서 또 다른 경로로 Flush가 호출되거나)
	// READY 이벤트가 중복으로 들어오는 상황에서 중복 실행을 막기 위한 가드
	if (bFlushingPendingPlayers)
	{
		return;
	}

	bFlushingPendingPlayers = true;

	// 대기열에 쌓여 있던 플레이어 수(Experience 로딩 전에는 스폰을 막고 여기 넣어둠)
	const int32 PendingCount = PendingStartPlayers.Num();

	// 이제 Experience가 READY라서 스폰 게이트를 해제(UNBLOCK)하고 Flush 시작
	UE_LOG(LogTemp, Warning,
		TEXT("[SpawnGate] UNBLOCKED -> FlushPendingPlayers Count=%d ExpLoaded=%d"),
		PendingCount, IsExperienceLoaded()
	);

	// PendingStartPlayers는 TWeakObjectPtr을 쓰는게 안전함:
	// - 대기 중에 PC가 Disconnect/Destroyed 될 수 있어서
	// - Strong reference를 들고 있으면 위험(댕글링/GC 문제)해질 수 있음
	for (TWeakObjectPtr<APlayerController>& WeakPC : PendingStartPlayers)
	{
		// Weak → Strong로 꺼내고 유효성 검사
		APlayerController* PC = WeakPC.Get();
		if (!IsValid(PC))
		{
			// 대기 중 접속 종료 등으로 사라진 PC는 스킵
			continue;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[READY] -> Super::HandleStartingNewPlayer PC=%s"),
			*GetNameSafe(PC)
		);

		// [중요] Super::HandleStartingNewPlayer 를 직접 호출하는 이유
		// - 우리가 오버라이드한 HandleStartingNewPlayer는 "Experience READY 전이면 막기" 로직이 있음
		// - READY가 된 지금은 "엔진 기본 흐름"을 그대로 타는 게 가장 안전함
		//
		// Super 내부에서 일반적으로 이어지는 플로우:
		// HandleStartingNewPlayer
		//   → RestartPlayer
		//      → SpawnDefaultPawnFor / ChoosePlayerStart
		//      → Possess
		//      → (Pawn BeginPlay 등 정상 라이프사이클)
		Super::HandleStartingNewPlayer_Implementation(PC);
	}

	// Flush가 끝났으니 대기열 비움
	// (같은 PC를 중복 처리하지 않도록)
	PendingStartPlayers.Reset();

	// 재진입 가드 해제
	bFlushingPendingPlayers = false;
}

void AMosesGameModeBase::HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience)
{
	// Base GM은 공통 처리 없음.
	// Lobby/Match 등 모드별로 override해서 사용.
}

FString AMosesGameModeBase::GetConnAddr(APlayerController* PC)
{
	if (!PC) return TEXT("PC=None");
	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		return Conn->LowLevelGetRemoteAddress(true); // ip:port
	}
	return TEXT("Conn=None");
}
