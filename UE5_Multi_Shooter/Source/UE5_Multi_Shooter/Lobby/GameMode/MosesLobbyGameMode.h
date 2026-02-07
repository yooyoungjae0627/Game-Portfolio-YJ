// ============================================================================
// UE5_Multi_Shooter/Lobby/GameMode/MosesLobbyGameMode.h  (FULL - UPDATED)
// - [ADD] Travel 1회 보장 가드 + URL 단일화
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MosesLobbyGameMode.generated.h"

class AMosesPlayerController;
class AMosesPlayerState;
class AMosesLobbyGameState;
class UMSCharacterCatalog;
class UMosesExperienceDefinition;

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameMode();

	// ---------------------------------------------------------------------
	// Engine
	// ---------------------------------------------------------------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience);

	virtual void GenericPlayerInitialization(AController* C) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// ---------------------------------------------------------------------
	// Start Game (서버 최종 판정 + Travel)
	// ---------------------------------------------------------------------
	void HandleStartMatchRequest(AMosesPlayerState* HostPS);

	// Debug/Exec endpoint
	UFUNCTION(Exec)
	void TravelToMatch();

protected:
	// ---------------------------------------------------------------------
	// Character Select (Server authoritative → PS single truth)
	// ---------------------------------------------------------------------
	void HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId);

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override; // [FIX]

private:
	// ---------------------------------------------------------------------
	// Travel (server single function)
	// ---------------------------------------------------------------------
	void ServerTravelToMatch();

	// ✅ [ADD] URL 단일화(여기서만 URL 만들고 전부 재사용)
	FString BuildMatchTravelURL() const;

	// ✅ [ADD] 중복 호출 증거 로그(원인 검증용)
	void LogTravelCall_Evidence(const TCHAR* From) const;

private:
	// ---------------------------------------------------------------------
	// Helpers (checked getters)
	// ---------------------------------------------------------------------
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const;

private:
	// ---------------------------------------------------------------------
	// Character catalog resolve
	// ---------------------------------------------------------------------
	int32 ResolveCharacterId(const FName CharacterId) const;

private:
	// ---------------------------------------------------------------------
	// Dev nickname
	// ---------------------------------------------------------------------
	void EnsureDevNickname_Server(AMosesPlayerState* PS) const;

private:
	// ---------------------------------------------------------------------
	// Tunables
	// ---------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Travel")
	bool bUseSeamlessTravelToMatch = true;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Travel")
	FName MatchLevelName = TEXT("MatchLevel");

	// 프로젝트에서 실제 맵이 /Game/Map/MatchLevel 인지 /Game/Maps/MatchLevel 인지 혼용 방지용
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Travel")
	FString MatchMapRootPath = TEXT("/Game/Map/"); // [MOD] 너 프로젝트 로그 기준 기본값

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Character")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

private:
	// ✅ [ADD] Travel 1회 보장 (중복 ServerTravel 차단)
	UPROPERTY(Transient)
	bool bTravelToMatchStarted = false;
};
