#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MosesLobbyGameMode.generated.h"

class AMosesPlayerController;
class AMosesPlayerState;
class AMosesLobbyGameState;
class UMSCharacterCatalog;

/**
 * AMosesLobbyGameMode
 *
 * 역할
 * - StartGame 요청의 "최종 검증문"
 * - 검증 OK면 ServerTravel로 Match 맵 이동 트리거
 *
 * 왜 GameState가 아니라 여기인가?
 * - Travel은 서버의 룰/흐름(Authority Rule)이라 GameMode 책임이 가장 안전
 * - GameState는 데이터 단일진실(룸/페이즈/리스트)
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameMode();

	// ---------------------------
	// Public Entry
	// ---------------------------

	/** PC가 Start 요청했을 때 호출(서버) */
	void HandleStartGameRequest(AMosesPlayerController* RequestPC);

	/** 디버그/테스트: 강제 Travel */
	void TravelToMatch();

	void HandleSelectCharacterRequest(class AMosesPlayerController* RequestPC, const FName CharacterId);

protected:
	virtual void BeginPlay() override;

private:
	// ---------------------------
	// Internal
	// ---------------------------
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const;

	void DoServerTravelToMatch();

	int32 ResolveCharacterId(const FName CharacterId) const;


	UPROPERTY(EditDefaultsOnly, Category = "CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	// ---------------------------
	// Config
	// ---------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	FString MatchMapTravelURL = TEXT("/Game/Maps/MatchMap?listen");

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	bool bUseSeamlessTravelToMatch = true;

};
