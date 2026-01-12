#include "MosesStartFlowSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "UE5_Multi_Shooter/MosesGameInstance.h"
#include "MosesUserSessionSubsystem.h"

void UMosesStartFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 맵 로드 후 한 틱 뒤에 UI 띄우는 게 가장 안전함(Experience/PC 안정화)
	if (UWorld* World = GetWorld())
	{
		// ✅ [권장] Subsystem이 Deinitialize 된 뒤에 람다가 실행될 수 있으니 WeakPtr로 안전 처리
		TWeakObjectPtr<UMosesStartFlowSubsystem> WeakThis(this);

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->TryShowStartUI();
				}
			}));
	}
}

void UMosesStartFlowSubsystem::Deinitialize()
{
	if (StartWidget)
	{
		StartWidget->RemoveFromParent();
		StartWidget = nullptr;
	}

	Super::Deinitialize();
}

void UMosesStartFlowSubsystem::TryShowStartUI()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ✅ [권장] PIE 접두사(예: UEDPIE_0_) 제거된 MapName을 쓰는게 안전
	const FString MapName = World->GetMapName().ToLower();
	const bool bIsStartLike = MapName.Contains(TEXT("start")) || MapName.Contains(TEXT("startgame"));
	if (!bIsStartLike)
	{
		return;
	}

	if (StartWidget)
	{
		return;
	}

	if (StartWidgetClass.IsNull())
	{
		// 최소한의 안전장치: 클래스 지정 안 했으면 아무 것도 안 함
		return;
	}

	UClass* LoadedClass = StartWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		return;
	}

	// =========================================================
	// ✅ [핵심 수정] CreateWidget의 OwningObject는 ULocalPlayer* 가 "지원 타입"이 아님
	//    -> APlayerController(추천) 또는 UWorld/UGameInstance로 만들어야 함
	// =========================================================

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	APlayerController* PC = LocalPlayer->GetPlayerController(World);
	if (!PC)
	{
		return;
	}

	StartWidget = CreateWidget<UUserWidget>(PC, LoadedClass);
	if (!StartWidget)
	{
		return;
	}

	// (선택) 혹시 모를 소유 로컬플레이어 보정
	StartWidget->SetOwningLocalPlayer(LocalPlayer);

	StartWidget->AddToViewport(10);
}

void UMosesStartFlowSubsystem::SubmitNicknameAndEnterLobby(const FString& Nickname)
{
	if (UGameInstance* GI = UMosesGameInstance::Get(this))
	{
		if (UMosesUserSessionSubsystem* Session = GI->GetSubsystem<UMosesUserSessionSubsystem>())
		{
			Session->SetNickname(Nickname);

			// ✅ 여기 주소만 네 로비 서버 주소로 바꿔라.
			// - 로컬 테스트: "LobbyLevel" (OpenLevel 사용)
			// - Dedicated 접속: "127.0.0.1:7777" 같은 접속 주소
			const FString LobbyAddr = TEXT("127.0.0.1:7777");

			// 닉네임을 옵션으로 같이 넘기기(서버에서 OptionsString으로 받기 가능)
			Session->TravelToLobby(LobbyAddr, FString::Printf(TEXT("Nick=%s"), *Nickname));
		}
	}
}
