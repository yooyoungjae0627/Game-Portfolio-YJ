#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "MosesUIRegistrySubsystem.generated.h"

/**
 * UMosesUIRegistrySubsystem
 *
 * 역할:
 * - GameFeature가 제공하는 UI SoftClassPath(경로)를 "전역 저장소"로 보관한다.
 * - GF Action은 여기로 "경로 등록/해제"만 한다. (CreateWidget 금지)
 * - LocalPlayerSubsystem이 여기서 경로를 읽어 실제 위젯을 생성/부착한다.
 *
 * 주의:
 * - 이 Subsystem은 "모든 월드 컨텍스트"에서 생성되는 게 아니라
 *   GameInstance에 종속이다. (GetSubsystem으로 얻는다)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesUIRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---------------------------
	// Public API
	// ---------------------------
	void SetLobbyWidgetClassPath(const FSoftClassPath& InPath);
	void ClearLobbyWidgetClassPath();
	FSoftClassPath GetLobbyWidgetClassPath() const;

private:
	// ---------------------------
	// State
	// ---------------------------
	UPROPERTY()
	FSoftClassPath LobbyWidgetClassPath;
};
