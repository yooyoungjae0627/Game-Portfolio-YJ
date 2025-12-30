#include "MosesLobbyLocalPlayerSubsystem.h"
#include "UE5_Multi_Shooter/UI/MosesLobbyWidget.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

/**
 * 에셋 경로 정책:
 * - WidgetClass는 반드시 *_C (GeneratedClass) 경로를 사용한다.
 * - IMC는 Object 경로 그대로 사용한다.
 *
 * ⚠ 경로 바꾸면:
 * - 패키징/리네임 시 깨지기 쉬움
 * - 가능하면 추후 SoftObjectPath/DataAsset로 교체 고려
 */
static constexpr const TCHAR* LobbyWidgetPath =
TEXT("/GF_Lobby/Lobby/UI/WBP_Lobby.WBP_Lobby_C");

static constexpr const TCHAR* LobbyIMCPath =
TEXT("/GF_Lobby/Lobby/Input/IMC_Lobby.IMC_Lobby");

void UMosesLobbyLocalPlayerSubsystem::EnsureAssetsLoaded()
{
	// IMC 로드: 실패하면 입력 적용이 불가능하므로 Error 로그 1회는 반드시 남긴다.
	if (!IMC_Lobby && !bTriedLoadIMC)
	{
		bTriedLoadIMC = true;

		IMC_Lobby = Cast<UInputMappingContext>(
			StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, LobbyIMCPath));

		if (!IMC_Lobby)
		{
			UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Load FAIL IMC=%s"), LobbyIMCPath);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Load OK IMC=%s"), LobbyIMCPath);
		}
	}

	// WidgetClass 로드: 실패하면 UI 생성이 불가능하다.
	if (!LobbyWidgetClass && !bTriedLoadWidgetClass)
	{
		bTriedLoadWidgetClass = true;

		UClass* Loaded = StaticLoadClass(UUserWidget::StaticClass(), nullptr, LobbyWidgetPath);
		if (!Loaded)
		{
			UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Load FAIL WidgetClass=%s"), LobbyWidgetPath);
		}
		else
		{
			LobbyWidgetClass = Loaded;
			UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Load OK WidgetClass=%s"), LobbyWidgetPath);
		}
	}
}

void UMosesLobbyLocalPlayerSubsystem::NotifyRoomStateChanged()
{
	// RepRoomList 갱신 등으로 UI만 다시 그리면 되는 경우.
	if (LobbyWidget.IsValid())
	{
		LobbyWidget->UpdateStartButton();
	}
}

void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] ActivateLobbyUI"));

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Activate FAIL: LocalPlayer=null"));
		return;
	}

	// UI/IMC 에셋 로드 보장
	EnsureAssetsLoaded();

	// 입력은 UI 생성 여부와 무관하게 먼저 적용(중복 호출 대비 플래그로 방어)
	AddLobbyMapping();

	// 이미 UI가 살아있으면 중복 생성 금지
	if (LobbyWidget.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Activate SKIP: Widget already valid"));
		return;
	}

	if (!LobbyWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Activate FAIL: LobbyWidgetClass=null"));
		return;
	}

	APlayerController* PC = LP->GetPlayerController(GetWorld());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Activate FAIL: PlayerController=null"));
		return;
	}

	// 위젯 생성 + 타입 캐스팅(구체 위젯 API 사용 목적)
	UUserWidget* RawWidget = CreateWidget<UUserWidget>(PC, LobbyWidgetClass);
	UMosesLobbyWidget* TypedWidget = Cast<UMosesLobbyWidget>(RawWidget);

	if (!TypedWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] Activate FAIL: Widget cast failed (UMosesLobbyWidget)"));
		if (RawWidget) RawWidget->RemoveFromParent();
		return;
	}

	TypedWidget->AddToViewport();
	LobbyWidget = TypedWidget;

	// 입력 모드/UI 포커스/마우스 정책: 로비에서는 UI 조작이 주이므로 GameAndUI 사용
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TypedWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);

	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Activate OK"));
}

void UMosesLobbyLocalPlayerSubsystem::DeactivateLobbyUI()
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] DeactivateLobbyUI"));

	// 로비 입력 제거(중복 호출 대비 플래그로 방어)
	RemoveLobbyMapping();

	// UI 제거(레벨 이동 중 호출될 수 있으므로 WeakPtr 유효성 체크)
	if (LobbyWidget.IsValid())
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Widget removed"));
	}

	// 입력 모드 원복: PC가 없으면(Travel 중) 안전하게 스킵
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Deactivate SKIP: LocalPlayer=null"));
		return;
	}

	APlayerController* PC = LP->GetPlayerController(GetWorld());
	if (!PC)
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] Deactivate SKIP: PlayerController=null"));
		return;
	}

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(false);

	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] InputMode restored (GameOnly)"));
}

void UMosesLobbyLocalPlayerSubsystem::AddLobbyMapping()
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] AddMapping SKIP: LocalPlayer=null"));
		return;
	}

	if (!IMC_Lobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] AddMapping SKIP: IMC_Lobby=null (load fail?)"));
		return;
	}

	// GameFeature 중복 활성/중복 호출 방어
	if (bLobbyMappingAdded)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EISub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!EISub)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] AddMapping FAIL: EnhancedInputLocalPlayerSubsystem=null"));
		return;
	}

	EISub->AddMappingContext(IMC_Lobby, /*Priority*/ 0);
	bLobbyMappingAdded = true;

	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] IMC added"));
}

void UMosesLobbyLocalPlayerSubsystem::RemoveLobbyMapping()
{
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] RemoveMapping SKIP: LocalPlayer=null"));
		return;
	}

	if (!IMC_Lobby)
	{
		// 로드 실패 or 이미 해제된 케이스 → 조용히 스킵
		return;
	}

	if (!bLobbyMappingAdded)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EISub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!EISub)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] RemoveMapping FAIL: EnhancedInputLocalPlayerSubsystem=null"));
		return;
	}

	EISub->RemoveMappingContext(IMC_Lobby);
	bLobbyMappingAdded = false;

	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] IMC removed"));
}

void UMosesLobbyLocalPlayerSubsystem::NotifyRoomCreated(const FGuid& NewRoomId)
{
	// UI가 아직 없으면(Room 생성 이벤트가 먼저 온 경우) 스킵
	if (!LobbyWidget.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyUI] NotifyRoomCreated SKIP: Widget invalid"));
		return;
	}

	LobbyWidget->SetRoomIdText(NewRoomId);

	UE_LOG(LogTemp, Log, TEXT("[LobbyUI] NotifyRoomCreated OK RoomId=%s"),
		*NewRoomId.ToString(EGuidFormats::DigitsWithHyphens));
}
