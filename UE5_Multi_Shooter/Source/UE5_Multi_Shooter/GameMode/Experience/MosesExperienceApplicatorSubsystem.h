#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MosesExperienceApplicatorSubsystem.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 * UMosesExperienceApplicatorSubsystem
 *
 * [한 줄 역할]
 * - 클라이언트 로컬에서 “Experience Payload”를 실제로 적용한다.
 *
 * [적용 대상]
 * - HUD 위젯 생성/부착
 * - Enhanced Input Mapping Context 적용
 *
 * [원칙]
 * - 서버 권위 구조: 서버는 결정/복제만 한다.
 * - 클라는 READY 이후 "적용/표시"만 한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesExperienceApplicatorSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// ----------------------------
	// Apply API
	// ----------------------------
	void ApplyCurrentExperiencePayload();
	void ClearApplied();

private:
	// ----------------------------
	// Internal helpers
	// ----------------------------
	void ApplyHUD();
	void ApplyInputMapping();

private:
	// ----------------------------
	// Applied runtime instances
	// ----------------------------
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SpawnedHUDWidget = nullptr;

	// NOTE: InputMapping은 EnhancedInputSubsystem을 통해 적용되므로
	//       우리가 인스턴스를 들고 있을 필요는 없지만,
	//       "Clear" 시 제거할 IMC를 기록해두면 안전하다.
	UPROPERTY(Transient)
	TObjectPtr<const UInputMappingContext> AppliedIMC = nullptr;

	UPROPERTY(Transient)
	int32 AppliedInputPriority = 0;
};
