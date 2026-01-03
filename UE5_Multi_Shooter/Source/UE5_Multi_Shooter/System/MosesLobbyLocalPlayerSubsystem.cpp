#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyWidget.h"
#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

void UMosesLobbyLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Initialize LP=%s"), *GetNameSafe(GetLocalPlayer()));
}

void UMosesLobbyLocalPlayerSubsystem::Deinitialize()
{
	DeactivateLobbyUI();

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Deinitialize LP=%s"), *GetNameSafe(GetLocalPlayer()));

	Super::Deinitialize();
}

// ---------------------------
// UI control
// ---------------------------

void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI ENTER"));

	// ================================
	// [PATH PROOF] 경로가 진짜인지 즉시 판결
	// ================================
	{
		const FSoftClassPath TestPath(
			TEXT("/GF_Lobby_Code/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C")
		);

		UClass* TestClass = TestPath.TryLoadClass<UUserWidget>();

		if (TestClass)
		{
			UE_LOG(LogMosesSpawn, Warning,
				TEXT("[PATH PROOF] OK : %s -> Class=%s"),
				*TestPath.ToString(),
				*GetNameSafe(TestClass));
		}
		else
		{
			UE_LOG(LogMosesSpawn, Error,
				TEXT("[PATH PROOF] FAIL : %s"),
				*TestPath.ToString());
		}
	}

	// 개발자 주석: 중복 생성 방지
	if (IsLobbyWidgetValid())
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI SKIP (AlreadyValid) Widget=%s"),
			*GetNameSafe(LobbyWidget));
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	UGameInstance* GI = LP ? LP->GetGameInstance() : nullptr;

	if (!GI)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[UIFlow] ActivateLobbyUI FAIL (No GameInstance)"));
		return;
	}

	UMosesUIRegistrySubsystem* Registry = GI->GetSubsystem<UMosesUIRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[UIFlow] ActivateLobbyUI FAIL (No UIRegistrySubsystem)"));
		return;
	}

	FSoftClassPath WidgetPath = Registry->GetLobbyWidgetClassPath();

	if (WidgetPath.IsNull())
	{
		// 임시 강제 경로 (GF 디버깅용)
		WidgetPath = FSoftClassPath(
			TEXT("/GF_Lobby_Code/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C")
		);

		UE_LOG(LogMosesSpawn, Warning,
			TEXT("[UIFlow] LobbyWidgetPath NULL → FORCE SET = %s"),
			*WidgetPath.ToString()
		);
	}

	// 개발자 주석:
	// - _C까지 들어간 "WidgetBlueprintGeneratedClass" 경로여야 TryLoadClass가 성공한다.
	// - 실패하면 99%가 마운트포인트(/GF_Lobby_Data vs /GF_Lobby) 문제다.
	UClass* LoadedClass = WidgetPath.TryLoadClass<UUserWidget>();
	if (!LoadedClass)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (TryLoadClass) Path=%s"),
			*WidgetPath.ToString());
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI LoadedClass=%s"), *GetNameSafe(LoadedClass));

	// 개발자 주석:
	// - WBP_LobbyPage의 ParentClass가 UMosesLobbyWidget 이어야 안전하다.
	// - ParentClass가 다르면 CreateWidget은 되더라도 Cast 실패/기능 불일치가 생긴다.
	LobbyWidget = CreateWidget<UMosesLobbyWidget>(GetWorld(), LoadedClass);
	if (!LobbyWidget)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (CreateWidget) Class=%s"),
			*GetNameSafe(LoadedClass));
		return;
	}

	// ZOrder는 필요하면 올려라(일단 0)
	LobbyWidget->AddToViewport(0);

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI OK Widget=%s InViewport=%d"),
		*GetNameSafe(LobbyWidget),
		LobbyWidget->IsInViewport() ? 1 : 0);
}

void UMosesLobbyLocalPlayerSubsystem::DeactivateLobbyUI()
{
	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] DeactivateLobbyUI ENTER"));

	if (!LobbyWidget)
	{
		UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] DeactivateLobbyUI SKIP (Null)"));
		return;
	}

	const bool bWasInViewport = LobbyWidget->IsInViewport();
	LobbyWidget->RemoveFromParent();
	LobbyWidget = nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] DeactivateLobbyUI OK WasInViewport=%d"), bWasInViewport ? 1 : 0);
}

// ---------------------------
// Notify
// ---------------------------

void UMosesLobbyLocalPlayerSubsystem::NotifyPlayerStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyPlayerStateChanged"));
	LobbyPlayerStateChangedEvent.Broadcast();
}

void UMosesLobbyLocalPlayerSubsystem::NotifyRoomStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyRoomStateChanged"));
	LobbyRoomStateChangedEvent.Broadcast();
}

// ---------------------------
// Internal helpers
// ---------------------------

bool UMosesLobbyLocalPlayerSubsystem::IsLobbyWidgetValid() const
{
	return (LobbyWidget != nullptr) && LobbyWidget->IsInViewport();
}
