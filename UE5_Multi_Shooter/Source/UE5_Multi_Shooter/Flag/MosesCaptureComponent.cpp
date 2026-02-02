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

void UMosesCaptureComponent::ServerOnCaptureSucceeded(AMosesFlagSpot* Spot, float CaptureElapsedSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AMosesPlayerState* PS = Cast<AMosesPlayerState>(GetOwner());
	if (!PS)
	{
		UE_LOG(LogMosesFlag, Warning, TEXT("[CAPTURE][SV] Success FAIL (NoPS) Spot=%s"), *GetNameSafe(Spot));
		return;
	}

	// ---------------------------------------------------------------------
	// 1) SSOT: Capture Count 증가 (RepNotify -> Delegate -> HUD)
	// ---------------------------------------------------------------------
	PS->ServerAddCapture(1);

	UE_LOG(LogMosesFlag, Warning, TEXT("[CAPTURE][SV] SSOT AddCapture -> PS=%s NewPSCaptures=%d"),
		*GetNameSafe(PS),
		PS->GetCaptures());

	// ---------------------------------------------------------------------
	// 2) SSOT: Score 증가 (HUD ScoreAmount가 이걸로 움직인다)
	//    - 네가 원하는 정책: "캡쳐 성공하면 ScoreAmount가 쌓인다"
	//    - 여기서는 1점씩 누적 (원하면 값만 바꾸면 됨)
	// ---------------------------------------------------------------------
	const int32 AddScore = 1;
	PS->ServerAddScore(AddScore, TEXT("CaptureSuccess"));

	// ---------------------------------------------------------------------
	// 3) 중앙 Announcement (HUD)
	//    - 너가 원하면 여기 텍스트를 "캡쳐 성공"으로 고정해서 보낼 수도 있음
	// ---------------------------------------------------------------------
	if (AMosesMatchGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMosesMatchGameState>() : nullptr)
	{
		const FText AnnText = FText::FromString(TEXT("캡쳐 성공"));
		GS->ServerStartAnnouncementText(AnnText, 4);

		UE_LOG(LogMosesFlag, Warning, TEXT("[ANN][SV] FlagCapture -> Text='%s' Dur=%d GS=%s"),
			*AnnText.ToString(), 4, *GetNameSafe(GS));
	}

	UE_LOG(LogMosesFlag, Warning, TEXT("[CAPTURE][SV] Success -> AddScore=%d PS=%s Spot=%s Time=%.2f"),
		AddScore,
		*GetNameSafe(PS),
		*GetNameSafe(Spot),
		CaptureElapsedSeconds);

	// ---------------------------------------------------------------------
	// 4) 캡처 상태 종료 (HUD Progress 끄기)
	// ---------------------------------------------------------------------
	ServerSetCapturing(Spot, false, 0.0f, 0.0f);
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
