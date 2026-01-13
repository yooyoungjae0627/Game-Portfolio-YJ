#include "MosesStartGamePageWidget.h"

#include "Components/EditableText.h"
#include "Components/Button.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"

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
	AMosesPlayerController* MosesPlayerController = GetMosesPC();
	if (!MosesPlayerController)
	{
		return;
	}

	const FString Nick = GetNicknameText();
	if (Nick.IsEmpty())
	{
		return;
	}

	// 1) 서버 권위로 닉네임 저장(PS DebugName) + LoggedIn 처리
	MosesPlayerController->Server_SetLobbyNickname(Nick);

	// 2) LobbyLevel로 이동
	//    - 여기서 Experience를 굳이 옵션으로 붙이지 않아도
	//      LobbyGM/GMBase가 맵명 fallback 또는 InitGame에서 강제 가능.
	MosesPlayerController->ClientTravelToLobbyLevel();
}

FString UMosesStartGamePageWidget::GetNicknameText() const
{
	return ET_Nickname ? ET_Nickname->GetText().ToString().TrimStartAndEnd() : FString();
}

AMosesPlayerController* UMosesStartGamePageWidget::GetMosesPC() const
{
	return Cast<AMosesPlayerController>(GetOwningPlayer());
}
