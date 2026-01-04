
#include "UE5_Multi_Shooter/System/MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/System/MosesUIRegistrySubsystem.h"

#include "UE5_Multi_Shooter/Player/MosesPlayerController.h"
#include "UE5_Multi_Shooter/Player/MosesPlayerState.h"

#include "UE5_Multi_Shooter/GameMode/GameState/MosesLobbyGameState.h" 

#include "UE5_Multi_Shooter/MosesLogChannels.h"
#include "UE5_Multi_Shooter/UI/Lobby/MosesLobbyWidget.h"

#include "UE5_Multi_Shooter/Lobby/LobbyPreviewActor.h"

#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

// =========================================================
// Engine
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Initialize LP=%s World=%s"),
		*GetNameSafe(GetLocalPlayer()),
		*GetNameSafe(GetWorld()));
}

void UMosesLobbyLocalPlayerSubsystem::Deinitialize()
{
	// 개발자 주석:
	// - 로컬플레이어가 내려갈 때 UI도 내려준다.
	DeactivateLobbyUI();

	CachedLobbyPreviewActor = nullptr;

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] LobbySubsys Deinitialize LP=%s"), *GetNameSafe(GetLocalPlayer()));

	Super::Deinitialize();
}

// =========================================================
// UI control
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI ENTER LP=%s World=%s"),
		*GetNameSafe(GetLocalPlayer()),
		*GetNameSafe(GetWorld()));

	// 개발자 주석:
	// - 디버깅용: 경로가 실제 로딩되는지 즉시 확인
	{
		const FSoftClassPath TestPath(TEXT("/GF_Lobby_Code/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C"));
		UClass* TestClass = TestPath.TryLoadClass<UUserWidget>();

		UE_LOG(LogMosesSpawn, Warning, TEXT("[PATH PROOF] %s : %s -> Class=%s"),
			TestClass ? TEXT("OK") : TEXT("FAIL"),
			*TestPath.ToString(),
			*GetNameSafe(TestClass));
	}

	// 개발자 주석:
	// - 중복 생성 방지
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
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No GameInstance)"));
		return;
	}

	UMosesUIRegistrySubsystem* Registry = GI->GetSubsystem<UMosesUIRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No UIRegistrySubsystem)"));
		return;
	}

	FSoftClassPath WidgetPath = Registry->GetLobbyWidgetClassPath();

	if (WidgetPath.IsNull())
	{
		// 개발자 주석:
		// - GF 디버깅/임시 강제 경로
		WidgetPath = FSoftClassPath(TEXT("/GF_Lobby_Code/Lobby/UI/WBP_LobbyPage.WBP_LobbyPage_C"));

		UE_LOG(LogMosesSpawn, Warning, TEXT("[UIFlow] LobbyWidgetPath NULL -> FORCE SET = %s"),
			*WidgetPath.ToString());
	}

	UClass* LoadedClass = WidgetPath.TryLoadClass<UUserWidget>();
	if (!LoadedClass)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (TryLoadClass) Path=%s"),
			*WidgetPath.ToString());
		return;
	}

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI LoadedClass=%s"), *GetNameSafe(LoadedClass));

	// 개발자 주석:
	// - CreateWidget의 OwningPlayer가 없으면 GetOwningLocalPlayer가 NULL 될 수 있다.
	// - LocalPlayerSubsystem에서 만들면 "LP의 PlayerController"를 OwningPlayer로 넣는 게 안전하다.
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (No MosesPC)"));
		return;
	}

	LobbyWidget = CreateWidget<UMosesLobbyWidget>(PC, LoadedClass);
	if (!LobbyWidget)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[UIFlow] ActivateLobbyUI FAIL (CreateWidget) Class=%s"),
			*GetNameSafe(LoadedClass));
		return;
	}

	LobbyWidget->AddToViewport(0);

	UE_LOG(LogMosesSpawn, Log, TEXT("[UIFlow] ActivateLobbyUI OK Widget=%s InViewport=%d OwningPC=%s"),
		*GetNameSafe(LobbyWidget),
		LobbyWidget->IsInViewport() ? 1 : 0,
		*GetNameSafe(PC));

	// 개발자 주석:
	// - UI가 뜨는 순간 현재 PS 상태(SelectedCharacterId)에 맞춰 프리뷰도 동기화
	RefreshLobbyPreviewCharacter_LocalOnly();
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

// =========================================================
// Notify
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::NotifyPlayerStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyPlayerStateChanged"));

	// 개발자 주석:
	// - 1) Widget이 UI를 갱신할 수 있도록 신호
	LobbyPlayerStateChangedEvent.Broadcast();

	// 개발자 주석:
	// - 2) SelectedCharacterId 변경이 오면 프리뷰도 즉시 갱신
	RefreshLobbyPreviewCharacter_LocalOnly();
}

void UMosesLobbyLocalPlayerSubsystem::NotifyRoomStateChanged()
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyRoomStateChanged"));
	LobbyRoomStateChangedEvent.Broadcast();
}

// =========================================================
// Lobby Preview (Public API - Widget에서 호출)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RequestNextCharacterPreview_LocalOnly()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharNext] ENTER Local=%d"), LocalPreviewSelectedId);

	LocalPreviewSelectedId = (LocalPreviewSelectedId >= 2) ? 1 : 2;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharNext] Local -> %d"), LocalPreviewSelectedId);

	ApplyPreviewBySelectedId_LocalOnly(LocalPreviewSelectedId);
}

void UMosesLobbyLocalPlayerSubsystem::RequestPrevCharacterPreview_LocalOnly()
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPrev] ENTER Local=%d"), LocalPreviewSelectedId);

	LocalPreviewSelectedId = (LocalPreviewSelectedId <= 1) ? 2 : 1;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPrev] Local -> %d"), LocalPreviewSelectedId);

	ApplyPreviewBySelectedId_LocalOnly(LocalPreviewSelectedId);
}

// =========================================================
// Internal helpers
// =========================================================

bool UMosesLobbyLocalPlayerSubsystem::IsLobbyWidgetValid() const
{
	return (LobbyWidget != nullptr) && LobbyWidget->IsInViewport();
}

// =========================================================
// Lobby Preview (Local only)
// =========================================================

void UMosesLobbyLocalPlayerSubsystem::RefreshLobbyPreviewCharacter_LocalOnly()
{
	UWorld* World = GetWorld();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] RefreshLobbyPreviewCharacter ENTER World=%s NetMode=%d"),
		*GetNameSafe(World),
		World ? (int32)World->GetNetMode() : -1);

	// 개발자 주석:
	// - DS는 화면 없음
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] Refresh SKIP (NoWorld or DedicatedServer)"));
		return;
	}

	const AMosesPlayerState* PS = GetMosesPS_LocalOnly();
	if (!PS)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] Refresh FAIL: PS NULL"));
		return;
	}

	int32 SelectedId = PS->GetSelectedCharacterId();
	if (SelectedId != 1 && SelectedId != 2)
	{
		UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] SelectedId invalid (%d) -> clamp to 1"), SelectedId);
		SelectedId = 1;
	}

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] Refresh -> Apply SelectedId=%d"), SelectedId);

	// 개발자 주석:
	// - 처음 UI 켤 때만 PS 기반으로 로컬값 초기화(디폴트=1)
	const int32 PSId = PS->GetSelectedCharacterId();
	if (PSId == 1 || PSId == 2)
	{
		LocalPreviewSelectedId = PSId;
	}

	ApplyPreviewBySelectedId_LocalOnly(SelectedId);
}

void UMosesLobbyLocalPlayerSubsystem::ApplyPreviewBySelectedId_LocalOnly(int32 SelectedId)
{
	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] ApplyPreviewBySelectedId ENTER SelectedId=%d"), SelectedId);

	ALobbyPreviewActor* PreviewActor = GetOrFindLobbyPreviewActor_LocalOnly();

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] PreviewActor=%s Cached=%s"),
		*GetNameSafe(PreviewActor),
		*GetNameSafe(CachedLobbyPreviewActor));

	if (!PreviewActor)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] FAIL: PreviewActor NULL (레벨에 배치 안됨/다른 월드?)"));
		return;
	}

	const ECharacterType Type = (SelectedId == 2) ? ECharacterType::TypeB : ECharacterType::TypeA;

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] ApplyVisual Type=%d"), (int32)Type);

	PreviewActor->ApplyVisual(Type);
}

ALobbyPreviewActor* UMosesLobbyLocalPlayerSubsystem::GetOrFindLobbyPreviewActor_LocalOnly()
{
	// ✅ 핵심 수정:
	// - 캐시가 있고 유효하면 그대로 반환
	if (CachedLobbyPreviewActor && IsValid(CachedLobbyPreviewActor))
	{
		UE_LOG(LogMosesSpawn, Verbose, TEXT("[CharPreview] GetOrFind CACHE HIT=%s"),
			*GetNameSafe(CachedLobbyPreviewActor));
		return CachedLobbyPreviewActor.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] GetOrFind FAIL: World NULL"));
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ALobbyPreviewActor::StaticClass(), Found);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] GetOrFind Search Count=%d"), Found.Num());

	if (Found.Num() <= 0)
	{
		UE_LOG(LogMosesSpawn, Error, TEXT("[CharPreview] GetOrFind FAIL: No ALobbyPreviewActor in this level"));
		return nullptr;
	}

	CachedLobbyPreviewActor = Cast<ALobbyPreviewActor>(Found[0]);

	UE_LOG(LogMosesSpawn, Warning, TEXT("[CharPreview] GetOrFind CACHE SET=%s"),
		*GetNameSafe(CachedLobbyPreviewActor));

	return CachedLobbyPreviewActor.Get();
}

// =========================================================
// Player access helper
// =========================================================

AMosesPlayerController* UMosesLobbyLocalPlayerSubsystem::GetMosesPC_LocalOnly() const
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	return Cast<AMosesPlayerController>(LP->PlayerController);
}

AMosesPlayerState* UMosesLobbyLocalPlayerSubsystem::GetMosesPS_LocalOnly() const
{
	AMosesPlayerController* PC = GetMosesPC_LocalOnly();
	if (!PC)
	{
		return nullptr;
	}

	return PC->GetPlayerState<AMosesPlayerState>();
}

void UMosesLobbyLocalPlayerSubsystem::NotifyJoinRoomResult(EMosesRoomJoinResult Result, const FGuid& RoomId)
{
	UE_LOG(LogMosesSpawn, Verbose, TEXT("[UIFlow] NotifyJoinRoomResult Result=%d Room=%s"),
		(int32)Result, *RoomId.ToString());

	LobbyJoinRoomResultEvent.Broadcast(Result, RoomId);
}