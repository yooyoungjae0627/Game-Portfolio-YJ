#include "MosesStartGamePageWidget.h"

#include "Components/EditableText.h"
#include "Components/Button.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

void UMosesStartGamePageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Enter)
	{
		BTN_Enter->OnClicked.AddDynamic(this, &ThisClass::OnClicked_Enter);
	}
}

void UMosesStartGamePageWidget::OnClicked_Enter()
{
	// 버튼 클릭 시
	if (ET_Nickname == nullptr)
	{
		return;
	}

	const FString Nick = ET_Nickname->GetText().ToString();

	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UMosesLobbyLocalPlayerSubsystem* LPS = LP->GetSubsystem<UMosesLobbyLocalPlayerSubsystem>())
		{
			LPS->RequestSetLobbyNickname_LocalOnly(Nick);
		}
	}

	AMosesPlayerController* PC = GetOwningPlayer<AMosesPlayerController>();
	if (PC)
	{
		PC->Server_RequestEnterLobby(Nick); // Travel 요청만 수행
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
