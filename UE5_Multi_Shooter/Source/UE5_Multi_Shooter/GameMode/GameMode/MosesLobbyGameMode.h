#pragma once

#include "GameFramework/GameModeBase.h"
#include "UE5_Multi_Shooter/MosesDialogueTypes.h"
#include "MosesLobbyGameMode.generated.h"

class AMosesPlayerController;
class AMosesPlayerState;
class AMosesLobbyGameState;
class UMSCharacterCatalog;

class APlayerController;

/**
 * AMosesLobbyGameMode
 *
 * - 서버 전용 "심판/규칙" 클래스.
 * - 클라가 어떤 버튼을 눌렀든,
 *   최종 '허용/거부/상태 변경'은 여기서 결정한다.
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

	// 개발자 주석:
	// - PlayerController가 "시작 요청"을 했을 때 들어오는 표준 진입점
	// - 내부에서 기존 네 StartGame 로직으로 위임하면 된다.
	void HandleStartGame(class AMosesPlayerController* RequestPC);

	// ---------------------------
	// Request handlers (PC가 Server RPC로 요청하면 여기가 받는다)
	// ---------------------------
	void HandleRequestEnterLobbyDialogue(APlayerController* RequestPC);
	void HandleRequestExitLobbyDialogue(APlayerController* RequestPC);

	// ---------------------------
	// Dialogue progression API 
	// ---------------------------
	void ServerAdvanceLine(int32 NextLineIndex, float Duration, bool bNPCSpeaking);
	void ServerSetSubState(int32 NewSubStateAsInt, float RemainingTime, bool bNPCSpeaking);

	// ---------------------------
	// Dialogue Request handlers
	// ---------------------------
	void HandleDialogueAdvanceLineRequest(APlayerController* RequestPC);
	void HandleDialogueSetFlowStateRequest(APlayerController* RequestPC, EDialogueFlowState NewState);

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	// ---------------------------
	// Internal
	// ---------------------------
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const;

	void DoServerTravelToMatch();

	int32 ResolveCharacterId(const FName CharacterId) const;

	// ---------------------------
	// Validate rules 
	// ---------------------------
	bool CanEnterLobbyDialogue(APlayerController* RequestPC) const;
	bool CanExitLobbyDialogue(APlayerController* RequestPC) const;


private:
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
