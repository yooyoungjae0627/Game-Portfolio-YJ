// ============================================================================
// UE5_Multi_Shooter/Match/UI/Match/MosesResultPopupWidget.cpp  (NEW)
// ============================================================================

#include "UE5_Multi_Shooter/Match/UI/Match/MosesResultPopupWidget.h" // ✅ 반드시 첫 include

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"

#include "UE5_Multi_Shooter/MosesPlayerController.h"
#include "UE5_Multi_Shooter/MosesPlayerState.h"
#include "UE5_Multi_Shooter/Match/GameState/MosesMatchGameState.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

void UMosesResultPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
	}
}

void UMosesResultPopupWidget::HandleConfirmClicked()
{
	AMosesPlayerController* PC = Cast<AMosesPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][CL] Confirm clicked but no owning PC (Widget=%s)"), *GetNameSafe(this));
		return;
	}

	UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][CL] Confirm clicked -> Server_RequestReturnToLobby PC=%s"), *GetNameSafe(PC));

	// 클라는 요청만, 서버가 검증 후 ServerTravel
	PC->Server_RequestReturnToLobby();
}

void UMosesResultPopupWidget::ApplyResultOnce(
	const FMosesMatchResultState& ResultState,
	const AMosesPlayerState* MyPS,
	const AMosesPlayerState* OppPS)
{
	// ------------------------------------------------------------
	// 0) 안전 가드
	// ------------------------------------------------------------
	if (!MyPS)
	{
		UE_LOG(LogMosesPhase, Warning, TEXT("[RESULT][CL] ApplyResultOnce FAIL (MyPS null)"));
		return;
	}

	// ------------------------------------------------------------
	// 1) 내/상대 값 읽기
	// - ID는 PersistentId String 사용(요구사항)
	// ------------------------------------------------------------
	const FString MyId = MyPS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens);
	const FString OppId = OppPS ? OppPS->GetPersistentId().ToString(EGuidFormats::DigitsWithHyphens) : FString(TEXT(""));

	// ------------------------------------------------------------
	// 2) Winner/Loser/Draw 결정
	// - Draw면 DRAW
	// - 아니면 WinnerPersistentId와 내 PersistentId 비교
	// ------------------------------------------------------------
	const bool bIsDraw = ResultState.bIsDraw;

	bool bIsWinner = false;
	if (!bIsDraw && !ResultState.WinnerPersistentId.IsEmpty())
	{
		bIsWinner = (ResultState.WinnerPersistentId == MyId);
	}

	const FString StateText =
		bIsDraw ? TEXT("DRAW") :
		(bIsWinner ? TEXT("WINNER") : TEXT("LOSER"));

	UE_LOG(LogMosesPhase, Warning,
		TEXT("[RESULT][CL] ApplyResultOnce Draw=%d WinnerPid=%s MyPid=%s IsWinner=%d"),
		bIsDraw ? 1 : 0,
		*ResultState.WinnerPersistentId,
		*MyId,
		bIsWinner ? 1 : 0);

	// ------------------------------------------------------------
	// 3) UI SetText (Tick/Binding 금지, 여기서 1회)
	// ------------------------------------------------------------
	SetText_String(Text_ResultState, StateText);

	SetText_String(Text_MyId, MyId);
	SetText_Int(Text_MyCaptures, MyPS->GetCaptures());
	SetText_Int(Text_MyZombieKills, MyPS->GetZombieKills());
	SetText_Int(Text_MyPvPKills, MyPS->GetPvPKills());
	SetText_Int(Text_MyTotalScore, MyPS->GetTotalScore());

	SetText_String(Text_OppId, OppId);
	SetText_Int(Text_OppCaptures, OppPS ? OppPS->GetCaptures() : 0);
	SetText_Int(Text_OppZombieKills, OppPS ? OppPS->GetZombieKills() : 0);
	SetText_Int(Text_OppPvPKills, OppPS ? OppPS->GetPvPKills() : 0);
	SetText_Int(Text_OppTotalScore, OppPS ? OppPS->GetTotalScore() : 0);
}

void UMosesResultPopupWidget::SetText_Int(UTextBlock* TB, int32 Value)
{
	if (!TB)
	{
		return;
	}

	TB->SetText(FText::AsNumber(Value));
}

void UMosesResultPopupWidget::SetText_String(UTextBlock* TB, const FString& Str)
{
	if (!TB)
	{
		return;
	}

	TB->SetText(FText::FromString(Str));
}
