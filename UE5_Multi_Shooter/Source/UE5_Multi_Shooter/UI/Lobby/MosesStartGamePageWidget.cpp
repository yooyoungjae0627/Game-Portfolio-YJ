#include "MosesStartGamePageWidget.h"

#include "Components/EditableText.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h" // [ADD] NextTick 재적용

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

UMosesStartGamePageWidget::UMosesStartGamePageWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMosesStartGamePageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// [ADD] 1) 즉시 한 번 적용
	ApplyCursorAndFocus();

	// [ADD] 2) 다음 프레임에 한 번 더 적용 (StartLevel에서 포커스가 늦게 잡히는 문제 해결)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ApplyCursorAndFocus_NextTick);
	}

	if (BTN_Enter)
	{
		BTN_Enter->OnClicked.AddDynamic(this, &ThisClass::OnClicked_Enter);
	}
}

void UMosesStartGamePageWidget::ApplyCursorAndFocus()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	PC->bShowMouseCursor = true;
	PC->bEnableClickEvents = true;
	PC->bEnableMouseOverEvents = true;

	// [핵심] UIOnly보다 GameAndUI가 "캡처/포커스" 이슈에 더 강함
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	if (ET_Nickname)
	{
		// 키보드 포커스를 텍스트 박스에 강제로 준다.
		InputMode.SetWidgetToFocus(ET_Nickname->TakeWidget());
	}

	PC->SetInputMode(InputMode);

	// [ADD] Slate 포커스가 씹힐 때 대비: 위젯 자체도 포커스 가능이면 포커스 요청
	// (UEditableText는 버전에 따라 TakeWidget 포커스만으로 부족한 경우가 있음)
	if (ET_Nickname)
	{
		ET_Nickname->SetKeyboardFocus();
	}

	bAppliedInputMode = true;
}

void UMosesStartGamePageWidget::ApplyCursorAndFocus_NextTick()
{
	// [ADD] 다음 프레임에 재적용(동일 로직). 실제로 이 한 번이 문제를 대부분 해결한다.
	ApplyCursorAndFocus();
}

void UMosesStartGamePageWidget::OnClicked_Enter()
{
	if (ET_Nickname == nullptr)
	{
		return;
	}

	const FString Nick = GetNicknameText();
	if (Nick.IsEmpty())
	{
		return;
	}

	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* LPS = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			LPS->RequestSetLobbyNickname_LocalOnly(Nick);
		}
	}

	if (AMosesPlayerController* PC = GetOwningPlayer<AMosesPlayerController>())
	{
		PC->Server_RequestEnterLobby(Nick);
	}
}

FString UMosesStartGamePageWidget::GetNicknameText() const
{
	return ET_Nickname ? ET_Nickname->GetText().ToString().TrimStartAndEnd() : FString();
}

AMosesPlayerController* UMosesStartGamePageWidget::GetMosesPC() const
{
	return Cast<AMosesPlayerController>(GetOwningPlayer());
}
