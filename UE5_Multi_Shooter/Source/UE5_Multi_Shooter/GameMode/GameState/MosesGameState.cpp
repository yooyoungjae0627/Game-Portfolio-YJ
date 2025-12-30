// Fill out your copyright notice in the Description page of Project Settings.

#include "MosesGameState.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceManagerComponent.h"

void AMosesGameState::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogMosesExp, Warning, TEXT("[GS] BeginPAMosesPlayerControlleray World=%s NetMode=%d"),
		*GetNameSafe(GetWorld()), (int32)GetNetMode());
}

AMosesGameState::AMosesGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	 ExperienceManagerComponent = CreateDefaultSubobject<UMosesExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}

// MosesGameState.cpp (ADD)

void AMosesGameState::ServerSetPhase(EMosesServerPhase NewPhase)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentPhase == NewPhase)
    {
        return;
    }

    CurrentPhase = NewPhase;

    // DoD 4번 라인: Lobby로 확정되는 순간을 서버 로그로 고정
    if (CurrentPhase == EMosesServerPhase::Lobby)
    {
        UE_LOG(LogMosesPhase, Log, TEXT("[PHASE] Current = Lobby"));
    }
    else if (CurrentPhase == EMosesServerPhase::Match)
    {
        UE_LOG(LogMosesPhase, Log, TEXT("[PHASE] Current = Match"));
    }
}

void AMosesGameState::OnRep_CurrentPhase()
{
    // 클라에서 UI 갱신용으로 쓰면 됨.
    // DoD는 "서버 단독 실행 로그"가 기준이라 여기선 필수 아님.
}

void AMosesGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMosesGameState, CurrentPhase);
}


