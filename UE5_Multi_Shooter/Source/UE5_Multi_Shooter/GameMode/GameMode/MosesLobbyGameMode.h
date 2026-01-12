#pragma once

#include "CoreMinimal.h"
#include "UE5_Multi_Shooter/GameMode/GameMode/MosesGameModeBase.h"
#include "UE5_Multi_Shooter/Dialogue/MosesDialogueTypes.h"
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
 */
UCLASS()
class UE5_MULTI_SHOOTER_API AMosesLobbyGameMode : public AMosesGameModeBase
{
	GENERATED_BODY()

public:
	AMosesLobbyGameMode();

	// ---------------------------
	// Start Game (Entry)
	// ---------------------------

	/** PC가 Start 요청했을 때 호출(서버) : 내부에서 PS 기반 StartMatch로 위임 */
	void HandleStartGameRequest(AMosesPlayerController* RequestPC);

	/** Host Start 요청 최종 진입점(서버 단일 엔트리) */
	void HandleStartMatchRequest(AMosesPlayerState* HostPS);

	/** 디버그/테스트: 강제 Travel */
	void TravelToMatch();

	// ---------------------------
	// Character Select (서버 확정 → PS 단일진실)
	// ---------------------------
	void HandleSelectCharacterRequest(AMosesPlayerController* RequestPC, const FName CharacterId);

	// ---------------------------
	// Dialogue Request handlers (PC Server RPC → GM → GS)
	// ---------------------------
	void HandleRequestEnterLobbyDialogue(APlayerController* RequestPC);
	void HandleRequestExitLobbyDialogue(APlayerController* RequestPC);

	// ---------------------------
	// Dialogue progression API (서버가 진행시키는 뼈대)
	// ---------------------------
	void ServerAdvanceLine(int32 NextLineIndex, float Duration, bool bNPCSpeaking);
	void ServerSetSubState(int32 NewSubStateAsInt, float RemainingTime, bool bNPCSpeaking);

	// ---------------------------
	// Dialogue Request handlers
	// ---------------------------
	void HandleDialogueAdvanceLineRequest(APlayerController* RequestPC);
	void HandleDialogueSetFlowStateRequest(APlayerController* RequestPC, EDialogueFlowState NewState);

	/** PC의 텍스트 명령 제출을 서버에서 승인/거부하고, 승인 시 GS에 적용 */
	void HandleSubmitDialogueCommand(APlayerController* RequestPC, EDialogueCommandType Type, uint16 ClientCommandSeq);

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 로비 맵 진입 시 Experience를 확정하기 위해 옵션 강제 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** (선택) Experience READY 이후 DoD/로비 초기화 훅 */
	virtual void HandleDoD_AfterExperienceReady(const UMosesExperienceDefinition* CurrentExperience) override;

private:
	// ---------------------------
	// Helpers (로그 포함 Checked Getters)
	// ---------------------------
	AMosesLobbyGameState* GetLobbyGameStateChecked_Log(const TCHAR* Caller) const;
	AMosesPlayerState* GetMosesPlayerStateChecked_Log(AMosesPlayerController* PC, const TCHAR* Caller) const;

	// ---------------------------
	// Travel (서버 이동 단일 함수)
	// ---------------------------
	void ServerTravelToMatch();

	// ---------------------------
	// Character Catalog Resolve
	// ---------------------------
	int32 ResolveCharacterId(const FName CharacterId) const;

	// ---------------------------
	// Validate rules
	// ---------------------------
	bool CanEnterLobbyDialogue(APlayerController* RequestPC) const;
	bool CanExitLobbyDialogue(APlayerController* RequestPC) const;

	bool Gate_AcceptClientCommand(APlayerController* RequestPC, uint16 ClientCommandSeq, FString& OutRejectReason);
	bool Gate_IsCommandAllowedInCurrentFlow(EDialogueCommandType Type, const FDialogueNetState& NetState, FString& OutRejectReason) const;

private:
	// =========================================================
	// Config / Assets
	// =========================================================

	UPROPERTY(EditDefaultsOnly, Category = "CharacterSelect")
	TObjectPtr<UMSCharacterCatalog> CharacterCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	FName MatchLevelName = TEXT("MatchLevel");

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	bool bUseSeamlessTravelToMatch = true;

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue|Gate")
	float CommandCooldownSec = 0.30f;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<APlayerController>, uint16> LastAcceptedClientSeqByPC;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<APlayerController>, float> LastAcceptedTimeByPC;
};
