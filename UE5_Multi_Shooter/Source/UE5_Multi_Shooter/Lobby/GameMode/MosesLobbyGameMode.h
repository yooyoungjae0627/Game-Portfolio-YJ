// ============================================================================
// UE5_Multi_Shooter/Lobby/GameMode/MosesLobbyGameMode.h  (FULL - REORDERED)
// - Engine → StartGame/Travel → CharacterSelect → Helpers → Tunables/State
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

	// =========================================================================
	// Engine
	// =========================================================================
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	// DoD hook (optional)
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience);

	virtual void GenericPlayerInitialization(AController* C) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// =========================================================================
	// Start Game (서버 최종 판정 + Travel)
	// =========================================================================
	void HandleStartMatchRequest(AMosesPlayerState* HostPS);

	UFUNCTION(Exec)
	void TravelToMatch();

protected:
	// =========================================================================
	// Character Select (Server authoritative → PS single truth)
	// =========================================================================
	void HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId);

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

private:
	// =========================================================================
	// Travel (server single function)
	// =========================================================================
	void ServerTravelToMatch();
	FString BuildMatchTravelURL() const;
	void LogTravelCall_Evidence(const TCHAR* From) const;

private:
	// =========================================================================
	// Helpers (checked getters)
	// =========================================================================
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const;

private:
	// =========================================================================
	// Character catalog resolve
	// =========================================================================
	int32 ResolveCharacterId(const FName CharacterId) const;

private:
	// =========================================================================
	// Dev nickname
	// =========================================================================
	void EnsureDevNickname_Server(AMosesPlayerState* PS) const;

private:
	// =========================================================================
	// Tunables
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Travel")
	bool bUseSeamlessTravelToMatch = true;

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Travel")
	FName MatchLevelName = TEXT("MatchLevel");

	// /Game/Map/ vs /Game/Maps/ 혼용 방지
	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Travel")
	FString MatchMapRootPath = TEXT("/Game/Map/");

	UPROPERTY(EditDefaultsOnly, Category = "Moses|Lobby|Character")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

private:
	// =========================================================================
	// Runtime state
	// =========================================================================
	UPROPERTY(Transient)
	bool bTravelToMatchStarted = false;
};
