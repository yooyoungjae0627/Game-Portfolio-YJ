// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesPlayerState.h"

#include "UE5_Multi_Shooter/GameMode/MosesExperienceManagerComponent.h"
#include "UE5_Multi_Shooter/GameMode/MosesGameModeBase.h"

void AMosesPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Experience 로딩 완료 이벤트에 콜백 등록(이미 로드면 즉시 호출됨)
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	check(GameState);

	UMosesExperienceManagerComponent* ExperienceManagerComponent =
		GameState->FindComponentByClass<UMosesExperienceManagerComponent>();
	check(ExperienceManagerComponent);

	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
		FOnMosesExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded)
	);
}

void AMosesPlayerState::OnExperienceLoaded(const UMosesExperienceDefinition* CurrentExperience)
{
	// GameMode가 결정한 PawnData를 PlayerState에 저장(플레이어 기준 확정)
	if (AMosesGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AMosesGameModeBase>())
	{
		const UMosesPawnData* NewPawnData = GameMode->GetPawnDataForController(GetOwningController());
		check(NewPawnData);

		SetPawnData(NewPawnData);
	}
}

void AMosesPlayerState::SetPawnData(const UMosesPawnData* InPawnData)
{
	check(InPawnData);
	check(!PawnData); // 최초 1회만 세팅

	PawnData = InPawnData;
}



