#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MosesGameInstance.generated.h"

/**
 *
 */
UCLASS()
class UE5_MULTI_SHOOTER_API UMosesGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** 어디서든 MosesGameInstance 얻기 (WorldContext 있으면 가장 안전) */
	UFUNCTION(BlueprintPure, Category = "Moses|GameInstance", meta = (WorldContext = "WorldContextObject"))
	static UMosesGameInstance* Get(const UObject* WorldContextObject);

	/** (옵션) 실패 시 로그 찍고 nullptr 반환 */
	static UMosesGameInstance* GetChecked(const UObject* WorldContextObject);

	/**
	 * UGameInstance's interfaces
	*/
	virtual void Init() override;
	virtual void Shutdown() override;


private:
	bool bDidServerBootLog = false;
	void TryLogServerBoot(UWorld* World, const UWorld::InitializationValues IVS);
};
