#include "MosesUserSessionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UMosesUserSessionSubsystem::SetNickname(const FString& InNickname)
{
	Nickname = InNickname;
}

void UMosesUserSessionSubsystem::TravelToLobby(const FString& LobbyAddress, const FString& Options)
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC) return;

	// 닉네임을 옵션으로 같이 넘겨도 됨 (서버에서 OptionsString으로 파싱 가능)
	// 예: LobbyAddress="127.0.0.1:7777" / Options="Nick=YJ"
	const FString FinalURL = Options.IsEmpty()
		? LobbyAddress
		: FString::Printf(TEXT("%s?%s"), *LobbyAddress, *Options);

	// 멀티(서버 접속) 플로우: ClientTravel 권장
	PC->ClientTravel(FinalURL, TRAVEL_Absolute);

	// 싱글/로컬 테스트만 하려면 이걸 써도 됨:
	// UGameplayStatics::OpenLevel(World, FName(TEXT("LobbyLevel")));
}
