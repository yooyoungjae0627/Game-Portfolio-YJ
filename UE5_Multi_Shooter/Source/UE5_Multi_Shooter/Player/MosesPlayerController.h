// MosesPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MosesPlayerController.generated.h"

UCLASS()
class UE5_MULTI_SHOOTER_API AMosesPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosesPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

public:
	// ---- Lobby에서 UI가 호출할 서버 RPC ----
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void Server_SetSelectedCharacterId(int32 InId);

	// ---- 디버그용 콘솔 ----
	UFUNCTION(Exec)
	void YJ_SetReady(int32 InReady01);

	UFUNCTION(Exec)
	void YJ_SetChar(int32 InId);

	// 클라에서 입력해도 서버로 "전달"되도록 구성 (서버 창 입력 불가 대비)
	UFUNCTION(Exec)
	void TravelToMatch_Exec();

	UFUNCTION(Exec)
	void TravelToLobby_Exec();

private:
	// ---- Travel 실제 실행은 서버 RPC에서만 ----
	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch();

	UFUNCTION(Server, Reliable)
	void Server_TravelToLobby();

	// 서버에서 GM에게 위임하는 실제 함수
	void DoServerTravelToMatch();
	void DoServerTravelToLobby();
};
