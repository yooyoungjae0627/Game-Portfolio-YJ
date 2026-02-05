// MosesPerfEvidenceHelper.cpp
#include "MosesPerfEvidenceHelper.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// 프로젝트 로그 인프라 (이미 존재한다고 가정)
// - 새 LogCategory 만들지 말 것(헌법)
#include "UE5_Multi_Shooter/MosesLogChannels.h"

static const TCHAR* StatKindToCommand(EMosesPerfStatKind Kind)
{
	switch (Kind)
	{
	case EMosesPerfStatKind::Unit:  return TEXT("stat unit");
	case EMosesPerfStatKind::Game:  return TEXT("stat game");
	case EMosesPerfStatKind::Anim:  return TEXT("stat anim");
	case EMosesPerfStatKind::Net:   return TEXT("stat net");
	case EMosesPerfStatKind::Gpu:   return TEXT("stat gpu");
	case EMosesPerfStatKind::Audio: return TEXT("stat audio");
	default:                        return TEXT("stat unit");
	}
}

static const TCHAR* StatKindToToken(EMosesPerfStatKind Kind)
{
	switch (Kind)
	{
	case EMosesPerfStatKind::Unit:  return TEXT("unit");
	case EMosesPerfStatKind::Game:  return TEXT("game");
	case EMosesPerfStatKind::Anim:  return TEXT("anim");
	case EMosesPerfStatKind::Net:   return TEXT("net");
	case EMosesPerfStatKind::Gpu:   return TEXT("gpu");
	case EMosesPerfStatKind::Audio: return TEXT("audio");
	default:                        return TEXT("unit");
	}
}

static const TCHAR* ScenarioToToken(EMosesPerfScenarioId Scenario)
{
	switch (Scenario)
	{
	case EMosesPerfScenarioId::A1: return TEXT("A1");
	case EMosesPerfScenarioId::A2: return TEXT("A2");
	case EMosesPerfScenarioId::A3: return TEXT("A3");
	case EMosesPerfScenarioId::A4: return TEXT("A4");
	case EMosesPerfScenarioId::A5: return TEXT("A5");
	default:                       return TEXT("A1");
	}
}

static const TCHAR* BeforeAfterToToken(EMosesPerfBeforeAfter BA)
{
	return (BA == EMosesPerfBeforeAfter::Before) ? TEXT("before") : TEXT("after");
}

void UMosesPerfEvidenceHelper::RunCapturePreset_Base(UObject* WorldContextObject)
{
	RunStat(WorldContextObject, EMosesPerfStatKind::Unit);
	RunStat(WorldContextObject, EMosesPerfStatKind::Game);
	RunStat(WorldContextObject, EMosesPerfStatKind::Anim);
	RunStat(WorldContextObject, EMosesPerfStatKind::Net);
}

void UMosesPerfEvidenceHelper::RunStat(UObject* WorldContextObject, EMosesPerfStatKind Kind)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID] RunStat failed: WorldContextObject=null"));
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID] RunStat failed: World=null"));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PC)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID] RunStat failed: PC=null"));
		return;
	}

	const TCHAR* Cmd = StatKindToCommand(Kind);
	PC->ConsoleCommand(Cmd, true);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID] ConsoleCommand: %s"), Cmd);
}

FString UMosesPerfEvidenceHelper::MakeEvidenceFilename(
	int32 DayNumber,
	EMosesPerfScenarioId Scenario,
	bool bIsDedicatedServer,
	EMosesPerfStatKind StatKind,
	EMosesPerfBeforeAfter BeforeAfter)
{
	const TCHAR* ScenarioToken = ScenarioToToken(Scenario);
	const TCHAR* DSOrClient = bIsDedicatedServer ? TEXT("DS") : TEXT("Client");
	const TCHAR* StatToken = StatKindToToken(StatKind);
	const TCHAR* BAToken = BeforeAfterToToken(BeforeAfter);

	// 규칙: D{Day}_{Scenario}_{DS/Client}_{stat}_{before/after}.png
	return FString::Printf(TEXT("D%02d_%s_%s_%s_%s.png"), DayNumber, ScenarioToken, DSOrClient, StatToken, BAToken);
}

void UMosesPerfEvidenceHelper::LogEvidenceHint(
	UObject* WorldContextObject,
	int32 DayNumber,
	EMosesPerfScenarioId Scenario,
	bool bIsDedicatedServer,
	EMosesPerfBeforeAfter BeforeAfter)
{
	// Base preset 4종에 대해 파일명 힌트 로그를 뽑아준다.
	const FString Unit = MakeEvidenceFilename(DayNumber, Scenario, bIsDedicatedServer, EMosesPerfStatKind::Unit, BeforeAfter);
	const FString Game = MakeEvidenceFilename(DayNumber, Scenario, bIsDedicatedServer, EMosesPerfStatKind::Game, BeforeAfter);
	const FString Anim = MakeEvidenceFilename(DayNumber, Scenario, bIsDedicatedServer, EMosesPerfStatKind::Anim, BeforeAfter);
	const FString Net  = MakeEvidenceFilename(DayNumber, Scenario, bIsDedicatedServer, EMosesPerfStatKind::Net,  BeforeAfter);

	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID] Save these screenshots (30s x3, avg+maxspike):"));
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID]  - %s"), *Unit);
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID]  - %s"), *Game);
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID]  - %s"), *Anim);
	UE_LOG(LogMosesPhase, Warning, TEXT("[PERF][EVID]  - %s"), *Net);
}
