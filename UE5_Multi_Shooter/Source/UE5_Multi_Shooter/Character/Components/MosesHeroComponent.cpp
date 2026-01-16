#include "UE5_Multi_Shooter/Character/Components/MosesHeroComponent.h"

#include "UE5_Multi_Shooter/Character/Components/MosesPawnExtensionComponent.h"
#include "UE5_Multi_Shooter/Character/Data/MosesPawnData.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraComponent.h"
#include "UE5_Multi_Shooter/Camera/MosesCameraMode.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UMosesHeroComponent::UMosesHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMosesHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = GetPawn<APawn>();
	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] BeginPlay Pawn=%s Local=%d"),
		*GetNameSafe(Pawn),
		IsLocalPlayerPawn() ? 1 : 0);

	// ✅ 로컬 플레이어만 카메라/입력 세팅
	if (!IsLocalPlayerPawn())
	{
		return;
	}

	// 1) 카메라 모드 델리게이트 바인딩
	if (UMosesCameraComponent* CamComp = UMosesCameraComponent::FindCameraComponent(Pawn))
	{
		if (!CamComp->DetermineCameraModeDelegate.IsBound())
		{
			CamComp->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
			UE_LOG(LogMosesSpawn, Warning, TEXT("[Hero] Bind DetermineCameraModeDelegate OK Pawn=%s"), *GetNameSafe(Pawn));
		}
	}

	// 2) 입력 초기화 (현재는 뼈대)
	InitializePlayerInput();
}

bool UMosesHeroComponent::IsLocalPlayerPawn() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return false;
	}

	const AController* Controller = Pawn->GetController();
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC && PC->IsLocalController();
}

TSubclassOf<UMosesCameraMode> UMosesHeroComponent::DetermineCameraMode() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return nullptr;
	}

	const UMosesPawnExtensionComponent* PawnExt = UMosesPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (!PawnExt)
	{
		return nullptr;
	}

	const UMosesPawnData* PawnData = PawnExt->GetPawnData<UMosesPawnData>();
	if (!PawnData)
	{
		return nullptr;
	}

	return PawnData->DefaultCameraMode;
}

void UMosesHeroComponent::InitializePlayerInput()
{
	// ✅ 네 프로젝트는 아직 InputConfig 바인딩 주석이 많으니
	//    여기서는 “로컬에서 입력 세팅을 시작했다” 정도만 로그로 남기고
	//    Day3에서 EnhancedInput 바인딩을 완성하면 됨.

	const APawn* Pawn = GetPawn<APawn>();
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[Hero] InitializePlayerInput Pawn=%s PC=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(PC));
}
