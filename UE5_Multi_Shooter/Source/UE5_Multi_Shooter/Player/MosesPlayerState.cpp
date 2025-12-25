#include "MosesPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "UE5_Multi_Shooter/Character/MosesPawnData.h"  

#include "UE5_Multi_Shooter/GameMode/MosesExperienceManagerComponent.h"
#include "UE5_Multi_Shooter/GameMode/MosesGameModeBase.h"

AMosesPlayerState::AMosesPlayerState()
{
	// 서버에서 생성될 때 PersistentId 부여
	if (!PersistentId.IsValid())
	{
		PersistentId = FGuid::NewGuid();
	}
}

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
	if (AMosesGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AMosesGameModeBase>())
	{
		AController* OwnerController = Cast<AController>(GetOwner()); // PlayerState Owner = Controller
		const UMosesPawnData* NewPawnData = GameMode->GetPawnDataForController(OwnerController);
		check(NewPawnData);

		SetPawnData(NewPawnData);

		UE_LOG(LogTemp, Warning, TEXT("[PS][PawnData] SetPawnData PS=%s PC=%s PawnData=%s"),
			*GetNameSafe(this), *GetNameSafe(OwnerController), *GetNameSafe(NewPawnData));
	}
}

void AMosesPlayerState::SetPawnData(const UMosesPawnData* InPawnData)
{
	check(InPawnData);

	// 최초 1회만 세팅하는 정책이면 그대로 유지
	if (PawnData)
	{
		return;
	}

	PawnData = InPawnData;
}

void AMosesPlayerState::ServerSetSelectedCharacterId(int32 InId)
{
	if (!HasAuthority())
	{
		return;
	}
	SelectedCharacterId = InId;
}

void AMosesPlayerState::ServerSetReady(bool bInReady)
{
	if (!HasAuthority())
	{
		return;
	}
	bReady = bInReady;
}

FString AMosesPlayerState::MakeDebugString() const
{
	return FString::Printf(
		TEXT("PS=%p PlayerId=%d PersistentId=%s DebugName=%s SelChar=%d Ready=%d PlayerName=%s"),
		this,
		GetPlayerId(),
		*PersistentId.ToString(EGuidFormats::DigitsWithHyphens),
		*DebugName,
		SelectedCharacterId,
		bReady ? 1 : 0,
		*GetPlayerName()
	);
}

void AMosesPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (AMosesPlayerState* Dest = Cast<AMosesPlayerState>(PlayerState))
	{
		Dest->PersistentId = PersistentId;
		Dest->DebugName = DebugName;
		Dest->SelectedCharacterId = SelectedCharacterId;
		Dest->bReady = bReady;
		// PawnData는 경험치 로딩 흐름에서 다시 잡힐 수 있으니 정책에 맞게 선택
	}
}

void AMosesPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (AMosesPlayerState* Src = Cast<AMosesPlayerState>(PlayerState))
	{
		PersistentId = Src->PersistentId;
		DebugName = Src->DebugName;
		SelectedCharacterId = Src->SelectedCharacterId;
		bReady = Src->bReady;
	}
}

void AMosesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMosesPlayerState, PersistentId);
	DOREPLIFETIME(AMosesPlayerState, DebugName);
	DOREPLIFETIME(AMosesPlayerState, SelectedCharacterId);
	DOREPLIFETIME(AMosesPlayerState, bReady);
}
