// ============================================================================
// MosesCaptureComponent.cpp (FULL)
// ============================================================================

#include "UE5_Multi_Shooter/Flag/MosesCaptureComponent.h"
#include "UE5_Multi_Shooter/Flag/MosesFlagSpot.h"

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

	Captures++;

	// BestCaptureTimeSeconds는 "최초" 또는 "더 빠른 기록"이면 갱신
	if (BestCaptureTimeSeconds <= 0.0f || CaptureTimeSeconds < BestCaptureTimeSeconds)
	{
		BestCaptureTimeSeconds = CaptureTimeSeconds;
	}

	// 캡처 상태 종료
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
