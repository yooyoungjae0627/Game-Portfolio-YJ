#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceApplicatorSubsystem.h"

#include "UE5_Multi_Shooter/GameMode/Experience/MosesExperienceRegistrySubsystem.h"

#include "UE5_Multi_Shooter/MosesGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void UMosesExperienceApplicatorSubsystem::ApplyCurrentExperiencePayload()
{
	/**
	 * 개발자 주석:
	 * - Experience READY 이후 호출된다는 전제.
	 * - “기존 적용을 지우고 → 현재 Experience 기준으로 다시 적용”이 가장 안전하다.
	 */
	ClearApplied();

	ApplyHUD();
	ApplyInputMapping();
}

void UMosesExperienceApplicatorSubsystem::ClearApplied()
{
	// ----------------------------
	// HUD 제거
	// ----------------------------
	if (SpawnedHUDWidget)
	{
		SpawnedHUDWidget->RemoveFromParent();
		SpawnedHUDWidget = nullptr;
	}

	// ----------------------------
	// InputMapping 제거
	// ----------------------------
	if (AppliedIMC)
	{
		if (UEnhancedInputLocalPlayerSubsystem* EIS = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			EIS->RemoveMappingContext(const_cast<UInputMappingContext*>(AppliedIMC.Get()));
		}

		AppliedIMC = nullptr;
		AppliedInputPriority = 0;
	}
}

void UMosesExperienceApplicatorSubsystem::ApplyHUD()
{
	UMosesGameInstance* GI = UMosesGameInstance::Get(this);
	if (!GI)
	{
		return;
	}

	UMosesExperienceRegistrySubsystem* Registry = GI->GetSubsystem<UMosesExperienceRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const TSoftClassPtr<UUserWidget> HUDClassSoft = Registry->GetHUDWidgetClass();
	if (HUDClassSoft.IsNull())
	{
		// HUD가 없는 Experience도 있을 수 있다(예: Dedicated Server 전용, 혹은 아주 단순한 페이즈).
		return;
	}

	UClass* HUDClass = HUDClassSoft.LoadSynchronous();
	if (!HUDClass)
	{
		return;
	}

	SpawnedHUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDClass);
	if (SpawnedHUDWidget)
	{
		SpawnedHUDWidget->AddToViewport();
	}
}

void UMosesExperienceApplicatorSubsystem::ApplyInputMapping()
{
	UMosesGameInstance* GI = UMosesGameInstance::Get(this);
	if (!GI)
	{
		return;
	}

	UMosesExperienceRegistrySubsystem* Registry = GI->GetSubsystem<UMosesExperienceRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const TSoftObjectPtr<UInputMappingContext> IMCSoft = Registry->GetInputMapping();
	if (IMCSoft.IsNull())
	{
		return;
	}

	UInputMappingContext* IMC = IMCSoft.LoadSynchronous();
	if (!IMC)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EIS = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!EIS)
	{
		return;
	}

	const int32 Priority = Registry->GetInputPriority();
	EIS->AddMappingContext(IMC, Priority);

	AppliedIMC = IMC;
	AppliedInputPriority = Priority;
}
