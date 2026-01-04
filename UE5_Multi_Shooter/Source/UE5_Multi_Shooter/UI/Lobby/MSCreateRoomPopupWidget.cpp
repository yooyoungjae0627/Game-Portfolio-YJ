#include "MSCreateRoomPopupWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"

void UMSCreateRoomPopupWidget::BindOnConfirm(FOnConfirmCreateRoom InOnConfirm)
{
	OnConfirmCreateRoom = InOnConfirm;
}

void UMSCreateRoomPopupWidget::BindOnCancel(FOnCancelCreateRoom InOnCancel)
{
	OnCancelCreateRoom = InOnCancel;
}

void UMSCreateRoomPopupWidget::InitDefaultValues(const FString& InDefaultTitle, int32 InDefaultMaxPlayers)
{
	DefaultTitle = InDefaultTitle;
	DefaultMaxPlayers = InDefaultMaxPlayers;
	bDefaultsApplied = false;
}

// ---------------------------
// Engine
// ---------------------------

void UMSCreateRoomPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 개발자 주석:
	// - BindWidget 실패는 WBP의 IsVariable 체크/이름 불일치가 대부분 원인
	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.RemoveAll(this);
		Btn_Confirm->OnClicked.AddDynamic(this, &UMSCreateRoomPopupWidget::OnClicked_Confirm);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.RemoveAll(this);
		Btn_Cancel->OnClicked.AddDynamic(this, &UMSCreateRoomPopupWidget::OnClicked_Cancel);
	}

	ApplyDefaultValuesIfNeeded();
}

void UMSCreateRoomPopupWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.RemoveAll(this);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.RemoveAll(this);
	}
}

// ---------------------------
// Internal
// ---------------------------

void UMSCreateRoomPopupWidget::OnClicked_Confirm()
{
	const FString RoomTitle = GetRoomTitleText();

	int32 MaxPlayers = 0;
	if (!TryParseMaxPlayers(MaxPlayers))
	{
		if (Txt_Error)
		{
			Txt_Error->SetText(
				FText::FromString(TEXT("인원은 2~4 사이의 숫자만 입력할 수 있습니다."))
			);
			Txt_Error->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	// 개발자 주석:
	// - UI 1차 방어 통과
	// - 서버에서 동일한 Clamp/Validate 로직을 반드시 다시 수행해야 함

	if (Txt_Error)
	{
		Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (OnConfirmCreateRoom.IsBound())
	{
		OnConfirmCreateRoom.Execute(RoomTitle, MaxPlayers);
	}
}

void UMSCreateRoomPopupWidget::OnClicked_Cancel()
{
	if (OnCancelCreateRoom.IsBound())
	{
		OnCancelCreateRoom.Execute();
	}
}

bool UMSCreateRoomPopupWidget::TryParseMaxPlayers(int32& OutMaxPlayers) const
{
	OutMaxPlayers = 0;

	if (!EditableText_MaxPlayers)
	{
		return false;
	}

	const FString RawText =
		EditableText_MaxPlayers->GetText().ToString().TrimStartAndEnd();

	if (RawText.IsEmpty())
	{
		return false;
	}

	// 숫자 파싱
	const int32 ParsedValue = FCString::Atoi(*RawText);

	// 개발자 주석:
	// - 0 이하, 5 이상은 UI 단계에서 컷
	// - 허용 범위: 2 ~ 4
	if (ParsedValue < 2 || ParsedValue > 4)
	{
		return false;
	}

	OutMaxPlayers = ParsedValue;
	return true;
}


FString UMSCreateRoomPopupWidget::GetRoomTitleText() const
{
	if (!EditableText_RoomTitle)
	{
		return FString();
	}

	return EditableText_RoomTitle->GetText().ToString().TrimStartAndEnd();
}

void UMSCreateRoomPopupWidget::ApplyDefaultValuesIfNeeded()
{
	if (bDefaultsApplied)
	{
		return;
	}

	if (EditableText_RoomTitle)
	{
		EditableText_RoomTitle->SetText(FText::FromString(DefaultTitle));
	}

	if (EditableText_MaxPlayers)
	{
		EditableText_MaxPlayers->SetText(FText::AsNumber(DefaultMaxPlayers));
	}

	if (Txt_Error)
	{
		Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
	}

	bDefaultsApplied = true;
}
