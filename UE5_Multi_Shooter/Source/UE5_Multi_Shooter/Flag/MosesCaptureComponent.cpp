// ============================================================================
// MosesCaptureComponent.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"

UMosesCaptureComponent::UMosesCaptureComponent()
{
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

	UE_LOG(LogMosesFlag, VeryVerbose, TEXT("[FLAG][SV] CaptureState PS=%s Capturing=%d Alpha=%.2f"),
		*GetNameSafe(GetOwner()), CaptureState.bCapturing ? 1 : 0, CaptureState.ProgressAlpha);

	BroadcastCaptureState(); // 서버도 즉시 HUD가 필요할 수 있어 브로드캐스트
}

void UMosesCaptureComponent::ServerOnCaptureSucceeded(AMosesFlagSpot* /*Spot*/, float CaptureTimeSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Captures++;

	// BestCaptureTimeSeconds는 "최초" 또는 "더 빠른 기록"이면 갱신
	if (BestCaptureTimeSeconds <= 0.0f || CaptureTimeSeconds < BestCaptureTimeSeconds)
	{
		BestCaptureTimeSeconds = CaptureTimeSeconds;
	}

	// 캡처 상태 종료
	CaptureState.bCapturing = false;
	CaptureState.ProgressAlpha = 0.0f;

	UE_LOG(LogMosesFlag, Log, TEXT("[FLAG][SV] Captures++ PS=%s Captures=%d Best=%.2f"),
		*GetNameSafe(GetOwner()), Captures, BestCaptureTimeSeconds);

	BroadcastCaptures();
	BroadcastCaptureState();
}

void UMosesCaptureComponent::OnRep_CaptureState()
{
	UE_LOG(LogMosesFlag, Verbose, TEXT("[FLAG][CL] OnRep_CaptureState PS=%s Capturing=%d Alpha=%.2f"),
		*GetNameSafe(GetOwner()), CaptureState.bCapturing ? 1 : 0, CaptureState.ProgressAlpha);

	BroadcastCaptureState();
}

void UMosesCaptureComponent::OnRep_Captures()
{
	UE_LOG(LogMosesFlag, Verbose, TEXT("[FLAG][CL] OnRep_Captures PS=%s Captures=%d Best=%.2f"),
		*GetNameSafe(GetOwner()), Captures, BestCaptureTimeSeconds);

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
