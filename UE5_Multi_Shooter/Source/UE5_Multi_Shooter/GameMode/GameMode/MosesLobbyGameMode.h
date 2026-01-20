// ============================================================================
// MosesLobbyGameMode.h
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesGameModeBase.h"
#include "MosesLobbyGameMode.generated.h"

class APlayerController;
class AMosesPlayerController;
class AMosesPlayerState;
class AMosesLobbyGameState;
class UMSCharacterCatalog;
class UMosesExperienceDefinition;

/**
 * Lobby GM
 * - Experience 시스템 기반(SpawnGate 포함) 프로젝트이므로 AMosesGameModeBase를 상속한다.
 * - InitGame에서 ?Experience=Exp_Lobby를 강제해 로비에선 항상 Exp_Lobby가 적용되게 만든다.
 * - Host Start 승인 시 MatchLevel로 ServerTravel 하되, ?Experience=Exp_Match 옵션을 붙인다.
 *
 * [DIALOGUE/VOICE DISABLED]
 * - CommandCooldownSec 등 대화/커맨드 게이트류는 본 정리본에서는 비활성 처리
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameMode();

public:
	/*====================================================
	= Start Game (Entry)
	====================================================*/
	void HandleStartMatchRequest(AMosesPlayerState* HostPS);
	void TravelToMatch();

public:
	/*====================================================
	= Character Select (Server authoritative → PS single truth)
	====================================================*/
	void HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId);

public:
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override; // [FIX]

protected:
	/*====================================================
	= Engine
	====================================================*/
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience) override;
	virtual void GenericPlayerInitialization(AController* C) override;

private:
	/*====================================================
	= Helpers (checked getters)
	====================================================*/
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const;

private:
	/*====================================================
	= Travel (server single function)
	====================================================*/
	void ServerTravelToMatch();

private:
	/*====================================================
	= Character catalog resolve
	====================================================*/
	int32 ResolveCharacterId(const FName CharacterId) const;

private:
	// ----------------------------
	// Dev Nick Policy (Server-only)
	// ----------------------------
	void EnsureDevNickname_Server(AMosesPlayerState* PS) const;

private:
	/*====================================================
	= Config / Assets
	====================================================*/
	UPROPERTY(EditDefaultsOnly, Category = "CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	FName MatchLevelName = TEXT("MatchLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	bool bUseSeamlessTravelToMatch = true;

private:
	/*====================================================
	= Anti-spam (example placeholders - 유지)
	====================================================*/
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<APlayerController>, uint16> LastAcceptedClientSeqByPC;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<APlayerController>, float> LastAcceptedTimeByPC;

private:
	// ----------------------------
	// Tunables
	// ----------------------------
	/** 닉네임이 비어있을 때 강제 주입할 기본 닉네임 */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Dev")
	FString DevFallbackNickname = TEXT("Dev_Moses");
};
