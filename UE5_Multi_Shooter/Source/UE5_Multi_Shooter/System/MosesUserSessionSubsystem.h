#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MosesUserSessionSubsystem.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesUserSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 닉네임 저장
	UFUNCTION(BlueprintCallable, Category="Moses|Session")
	void SetNickname(const FString& InNickname);

	UFUNCTION(BlueprintCallable, Category="Moses|Session")
	const FString& GetNickname() const { return Nickname; }

	// Lobby로 이동 (서버 접속형 / 로컬 OpenLevel 둘 다 지원)
	UFUNCTION(BlueprintCallable, Category="Moses|Session")
	void TravelToLobby(const FString& LobbyAddress, const FString& Options = TEXT(""));

private:
	UPROPERTY()
	FString Nickname;
};
