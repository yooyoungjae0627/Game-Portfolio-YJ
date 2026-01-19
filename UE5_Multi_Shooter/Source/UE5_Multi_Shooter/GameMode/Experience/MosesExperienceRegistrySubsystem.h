#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "MosesExperienceRegistrySubsystem.generated.h"

class UUserWidget;
class UInputMappingContext;
class UMosesExperienceDefinition;

/**
 * UMosesExperienceRegistrySubsystem
 *
 * [한 줄 역할]
 * - “현재 Experience가 요구하는 것(HUD/IMC/AbilitySetPath)”을 한 곳에 저장하는 레지스트리
 *
 * [왜 필요?]
 * - ExperienceDefinition은 데이터이고,
 * - 실제 적용은 로컬 플레이어/PC/HUD/GAS 레이어가 한다.
 * - 그러면 “지금 Experience가 뭘 원하지?”를 조회할 수 있는 중앙 저장소가 필요하다.
 *
 * [원칙]
 * - Registry는 '적용'하지 않는다. (위젯 생성/입력 적용/Ability 부여 X)
 * - 오직 '보관/조회'만 담당한다.
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesExperienceRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ----------------------------
	// Registry API
	// ----------------------------
	void SetFromExperience(const UMosesExperienceDefinition* Experience);
	void Clear();

	// ----------------------------
	// Getter
	// ----------------------------
	TSoftClassPtr<UUserWidget> GetStartWidgetClass() const { return StartWidgetClass; }
	TSoftClassPtr<UUserWidget> GetHUDWidgetClass() const { return HUDWidgetClass; }

	TSoftObjectPtr<UInputMappingContext> GetInputMapping() const { return InputMapping; }
	int32 GetInputPriority() const { return InputPriority; }

	FSoftObjectPath GetAbilitySetPath() const { return AbilitySetPath; }

private:
	// ----------------------------
	// Payload 저장(Soft 참조)
	// ----------------------------
	TSoftClassPtr<UUserWidget> StartWidgetClass;
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	TSoftObjectPtr<UInputMappingContext> InputMapping;
	int32 InputPriority = 100;

	FSoftObjectPath AbilitySetPath;
};
