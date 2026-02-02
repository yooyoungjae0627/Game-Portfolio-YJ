// ============================================================================
// MosesCaptureComponent.cpp (FULL - UPDATED)
// - [MOD] Capture 성공 시 SSOT(PlayerState) Captures를 증가시켜 HUD가 갱신되도록 연결
// ============================================================================

#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"
#include "UE5_Multi_Shooter/GameMode/GameState/MosesMatchGameState.h"

#include "Net/UnrealNetwork.h"

UMosesCaptureComponent::UMosesCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMosesCaptureComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMosesCaptureComponent, CaptureState);
	DOREPLIFETIME(UMosesCaptureComponent, Captures);
	DOREPLIFETIME(UMosesCaptureComponent, BestCaptureTimeSeconds);
}

void UMosesCaptureComponent::ServerSetCapturing(AMosesFlagSpot* Spot, bool bNewCapturing, float NewProgressAlpha, float HoldSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	CaptureState.bCapturing = bNewCapturing;
	CaptureState.ProgressAlpha = FMath::Clamp(NewProgressAlpha, 0.0f, 1.0f);
	CaptureState.HoldSeconds = HoldSeconds;
	CaptureState.Spot = Spot;

	UE_LOG(LogMosesFlag, VeryVerbose, TEXT("[CAPTURE][SV] State PS=%s Capturing=%d Alpha=%.2f Hold=%.2f Spot=%s"),
		*GetNameSafe(GetOwner()),
		CaptureState.bCapturing ? 1 : 0,
		CaptureState.ProgressAlpha,
		CaptureState.HoldSeconds,
		*GetNameSafe(Spot));

	// 서버도 즉시 HUD(로컬 디버그 등)가 필요할 수 있어 브로드캐스트
	BroadcastCaptureState();
}

void UMosesCaptureComponent::ServerOnCaptureSucceeded(AMosesFlagSpot* Spot, float CaptureTimeSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// --------------------------------------------------------------------
	// Component 내부 통계 (기존 유지)
	// - OnCapturesChanged(Captures, BestTime)로 위젯/피드백에서 활용 가능
	// --------------------------------------------------------------------
	Captures++;

	// BestCaptureTimeSeconds는 "최초" 또는 "더 빠른 기록"이면 갱신
	if (BestCaptureTimeSeconds <= 0.0f || CaptureTimeSeconds < BestCaptureTimeSeconds)
	{
		BestCaptureTimeSeconds = CaptureTimeSeconds;
	}

	// --------------------------------------------------------------------
	// ✅ [MOD] SSOT(PlayerState) Captures++ (HUD 숫자 갱신의 핵심)
	// - HUD CapturesAmount는 UMosesCaptureComponent::Captures가 아니라
	//   AMosesPlayerState::Captures(RepNotify->Delegate)를 보고 갱신한다.
	// --------------------------------------------------------------------
	if (AMosesPlayerState* PS = Cast<AMosesPlayerState>(GetOwner()))
	{
		PS->ServerAddCapture(1);

		UE_LOG(LogMosesFlag, Log, TEXT("[CAPTURE][SV] SSOT AddCapture -> PS=%s NewPSCaptures=%d"),
			*GetNameSafe(PS),
			PS->GetCaptures());
	}
	else
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("[CAPTURE][SV] Owner is not AMosesPlayerState. HUD CapturesAmount will NOT update. Owner=%s"),
			*GetNameSafe(GetOwner()));
	}

	// --------------------------------------------------------------------
	// ✅ [MOD] HUD Announcement (두 클라 동일) — GameState가 복제한다.
	// - 4초간 "캡처 완료" 배너를 띄운다.
	// --------------------------------------------------------------------
	UWorld* World = GetWorld();
	AMosesMatchGameState* GS = World ? World->GetGameState<AMosesMatchGameState>() : nullptr;
	if (GS && GS->HasAuthority())
	{
		// 문구는 원하는 대로 변경 가능
		const FText AnnText = FText::FromString(TEXT("캡처 완료!"));
		const int32 DurationSeconds = 4;

		GS->ServerStartAnnouncementText(AnnText, DurationSeconds);

		UE_LOG(LogMosesFlag, Log, TEXT("[ANN][SV] FlagCapture -> Text='Capture Complete' Dur=%d GS=%s"),
			DurationSeconds,
			*GetNameSafe(GS));
	}
	else
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("[ANN][SV] GameState not found. Announcement will NOT show. World=%s"),
			*GetNameSafe(World));
	}

	// --------------------------------------------------------------------
	// 캡처 상태 종료
	// --------------------------------------------------------------------
	CaptureState.bCapturing = false;
	CaptureState.ProgressAlpha = 0.0f;
	CaptureState.Spot = Spot;

	UE_LOG(LogMosesFlag, Log, TEXT("[CAPTURE][SV] Success PS=%s Captures=%d Best=%.2f Time=%.2f Spot=%s"),
		*GetNameSafe(GetOwner()),
		Captures,
		BestCaptureTimeSeconds,
		CaptureTimeSeconds,
		*GetNameSafe(Spot));

	BroadcastCaptures();
	BroadcastCaptureState();
}

void UMosesCaptureComponent::OnRep_CaptureState()
{
	UE_LOG(LogMosesFlag, Verbose, TEXT("[CAPTURE][CL] OnRep_State PS=%s Capturing=%d Alpha=%.2f Hold=%.2f Spot=%s"),
		*GetNameSafe(GetOwner()),
		CaptureState.bCapturing ? 1 : 0,
		CaptureState.ProgressAlpha,
		CaptureState.HoldSeconds,
		*GetNameSafe(CaptureState.Spot.Get()));

	BroadcastCaptureState();
}

void UMosesCaptureComponent::OnRep_Captures()
{
	UE_LOG(LogMosesFlag, Verbose, TEXT("[CAPTURE][CL] OnRep_Captures PS=%s Captures=%d Best=%.2f"),
		*GetNameSafe(GetOwner()),
		Captures,
		BestCaptureTimeSeconds);

	BroadcastCaptures();
}

void UMosesCaptureComponent::BroadcastCaptureState()
{
	OnCaptureStateChanged.Broadcast(
		CaptureState.bCapturing,
		CaptureState.ProgressAlpha,
		CaptureState.HoldSeconds,
		CaptureState.Spot);
}

void UMosesCaptureComponent::BroadcastCaptures()
{
	OnCapturesChanged.Broadcast(Captures, BestCaptureTimeSeconds);
}
