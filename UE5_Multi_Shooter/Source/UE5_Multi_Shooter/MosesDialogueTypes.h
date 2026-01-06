#pragma once

#include "MosesDialogueTypes.generated.h"

/**
 * EGamePhase
 *
 * 개발자 주석:
 * - "모든 클라이언트가 반드시 같은 화면 흐름을 봐야 하는" 전역 단계.
 * - 로비에서 '대화 모드'로 들어가는 것은 모두에게 공유되어야 하므로
 *   반드시 GameState(복제 전광판)에 넣는다.
 */
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Lobby        UMETA(DisplayName="Lobby"),
	LobbyDialogue UMETA(DisplayName="LobbyDialogue"),
};

/**
 * EDialogueSubState
 *
 * 개발자 주석:
 * - 대화 모드 안에서의 세부 상태.
 *   Listening / Speaking 정도만으로도 "연출/카메라/UI" 분기가 충분하다.
 */
UENUM(BlueprintType)
enum class EDialogueSubState : uint8
{
	None      UMETA(DisplayName="None"),
	Listening UMETA(DisplayName="Listening"),
	Speaking  UMETA(DisplayName="Speaking"),
};

/**
 * FDialogueNetState
 *
 * 개발자 주석:
 * - "대화 진행 상태" 전광판.
 * - 서버가 유일하게 수정하고(GameMode/GameState),
 *   클라는 복제된 값을 보고 "표시/연출"만 한다.
 *
 * 중요:
 * - 이 구조 덕분에 LateJoin(늦게 들어온 사람)도
 *   현재 LineIndex/SubState/RemainingTime을 즉시 받으면 화면이 복구된다.
 */
USTRUCT(BlueprintType)
struct FDialogueNetState
{
	GENERATED_BODY()

	// 현재 대화 서브상태 (서버가 확정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EDialogueSubState SubState = EDialogueSubState::None;

	// 현재 대사 줄 번호(인덱스). 서버가 다음 줄을 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 LineIndex = 0;

	// 현재 서브상태/대사에서 남은 시간(타이머 연출용).
	// - 일단 "상태가 바뀌었다"를 느끼게 하는 용도(연출/디버그)로 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RemainingTime = 0.0f;

	// NPC가 말하는지(혹은 플레이어가 말하는지) 같은 연출 플래그.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bNPCSpeaking = true;
};
