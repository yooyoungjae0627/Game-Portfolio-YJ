#include "MosesLobbyLocalPlayerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

/**
 * ⚠ 경로 규칙
 * - Widget: 반드시 _C (GeneratedClass) 경로
 * - IMC: Object 경로 그대로 사용
 */
static constexpr const TCHAR* LobbyWidgetPath =
TEXT("/GF_Lobby/Lobby/UI/WBP_Lobby.WBP_Lobby_C");

static constexpr const TCHAR* LobbyIMCPath =
TEXT("/GF_Lobby/Lobby/Input/IMC_Lobby.IMC_Lobby");

/**
 * EnsureAssetsLoaded
 *
 * - Lobby에서 사용하는 IMC / Widget 에셋을
 *   StaticLoad 기반으로 보장 로드
 *
 * - 실패 시:
 *   · 즉시 nullptr 반환
 *   · 반드시 Error 로그로 원인 노출
 *
 * - bTriedLoad 플래그:
 *   · 동일 실패 로그 스팸 방지
 */
void UMosesLobbyLocalPlayerSubsystem::EnsureAssetsLoaded()
{
	if (!IMC_Lobby && !bTriedLoadIMC)
	{
		bTriedLoadIMC = true;

		IMC_Lobby = Cast<UInputMappingContext>(
			StaticLoadObject(
				UInputMappingContext::StaticClass(),
				nullptr,
				LobbyIMCPath
			)
		);

		if (!IMC_Lobby)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[LobbyUI] Failed to load IMC: %s"), LobbyIMCPath);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[LobbyUI] Loaded IMC: %s"), LobbyIMCPath);
		}
	}
}

/**
 * ActivateLobbyUI
 *
 * 🎯 목적
 * - 로비 화면 진입 시
 *   · UI 표시
 *   · UI 입력 활성화
 *   · 마우스 사용 가능 상태로 전환
 *
 * ❌ 실패 가능 포인트
 * - LocalPlayer 없음
 * - PlayerController 없음
 * - Widget / IMC 로드 실패
 *
 * → 모든 실패 지점에 원인 로그 필수
 */
void UMosesLobbyLocalPlayerSubsystem::ActivateLobbyUI()
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] ActivateLobbyUI()"));

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LobbyUI] ActivateLobbyUI REJECT: No LocalPlayer"));
		return;
	}

	// 에셋 보장 로드 + 입력 매핑 적용
	EnsureAssetsLoaded();
	AddLobbyMapping();

	// UI 중복 생성 방지
	if (LobbyWidget.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyUI] ActivateLobbyUI SKIP: LobbyWidget already exists"));
		return;
	}

	// Widget Class 로드
	UClass* WidgetClass =
		StaticLoadClass(UUserWidget::StaticClass(), nullptr, LobbyWidgetPath);

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LobbyUI] Failed to load widget class: %s"), LobbyWidgetPath);
		return;
	}

	APlayerController* PC = LP->GetPlayerController(GetWorld());
	if (!PC)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LobbyUI] ActivateLobbyUI REJECT: No PlayerController"));
		return;
	}

	// Widget 인스턴스 생성
	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LobbyUI] ActivateLobbyUI REJECT: CreateWidget failed"));
		return;
	}

	Widget->AddToViewport();
	LobbyWidget = Widget;

	/**
	 * 입력 모드 설정
	 *
	 * 핵심 포인트:
	 * - SetWidgetToFocus:
	 *   → UI 클릭 안 먹는 문제 방지
	 *
	 * - DoNotLock + HideCursor=false:
	 *   → 로비에서 마우스 자유 이동
	 */
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);

	UE_LOG(LogTemp, Warning,
		TEXT("[LobbyUI] Lobby UI successfully activated"));
}

/**
 * DeactivateLobbyUI
 *
 * 🎯 목적
 * - 로비 종료 / 매치 진입 시
 *   · UI 제거
 *   · Lobby 입력 제거
 *   · GameOnly 입력 모드로 복귀
 *
 * - 레벨 이동 중 호출될 수 있으므로
 *   모든 객체 접근은 유효성 체크 필수
 */
void UMosesLobbyLocalPlayerSubsystem::DeactivateLobbyUI()
{
	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] DeactivateLobbyUI()"));

	RemoveLobbyMapping();

	// UI 제거
	if (LobbyWidget.IsValid())
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;

		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyUI] Lobby UI removed"));
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyUI] DeactivateLobbyUI: LobbyWidget already invalid"));
	}

	// 입력 모드 원복
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyUI] DeactivateLobbyUI: No LocalPlayer"));
		return;
	}

	APlayerController* PC = LP->GetPlayerController(GetWorld());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LobbyUI] DeactivateLobbyUI: No PlayerController"));
		return;
	}

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(false);

	UE_LOG(LogTemp, Warning,
		TEXT("[LobbyUI] Input mode restored (GameOnly)"));
}

void UMosesLobbyLocalPlayerSubsystem::AddLobbyMapping()
{
	ULocalPlayer* LP = GetLocalPlayer();

	if (!LP)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] AddLobbyMapping SKIP: No LocalPlayer"));
		return;
	}

	if (!IMC_Lobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] AddLobbyMapping SKIP: IMC_Lobby is null (load failed?)"));
		return;
	}

	if (bLobbyMappingAdded)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] AddLobbyMapping SKIP: already added"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EISub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!EISub)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] AddLobbyMapping FAIL: No EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	EISub->AddMappingContext(IMC_Lobby, /*Priority*/0);
	bLobbyMappingAdded = true;

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] Lobby IMC added"));
}

void UMosesLobbyLocalPlayerSubsystem::RemoveLobbyMapping()
{
	ULocalPlayer* LP = GetLocalPlayer();

	if (!LP)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] RemoveLobbyMapping SKIP: No LocalPlayer"));
		return;
	}

	if (!IMC_Lobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] RemoveLobbyMapping SKIP: IMC_Lobby is null"));
		return;
	}

	if (!bLobbyMappingAdded)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] RemoveLobbyMapping SKIP: not added"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EISub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!EISub)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyUI] RemoveLobbyMapping FAIL: No EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	EISub->RemoveMappingContext(IMC_Lobby);
	bLobbyMappingAdded = false;

	UE_LOG(LogTemp, Warning, TEXT("[LobbyUI] Lobby IMC removed"));
}
