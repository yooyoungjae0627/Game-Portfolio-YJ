#pragma once

#include "UObject/ObjectPtr.h"
#include "Sound/SoundBase.h"
#include "MosesDialogueTypes.generated.h"

/**
 * EGamePhase
 * - "모든 클라이언트가 반드시 같은 화면 흐름을 봐야 하는" 전역 단계.
 * - 로비에서 '대화 모드'로 들어가는 것은 모두에게 공유되어야 하므로
 *   반드시 GameState(복제 전광판)에 넣는다.
 */
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Lobby			UMETA(DisplayName = "Lobby"),
	LobbyDialogue	UMETA(DisplayName = "LobbyDialogue"),
};

/**
 * EDialogueFlowState
 * - 서버 타이머/라인 진행에 직접 영향을 주는 "머신 상태".
 * - Speaking일 때만 RemainingTime이 감소한다 (Day8 정답화).
 */
UENUM(BlueprintType)
enum class EDialogueFlowState : uint8
{
	None			UMETA(DisplayName = "None"),
	Speaking		UMETA(DisplayName = "Speaking"),     // 타이머 감소 O
	Paused			UMETA(DisplayName = "Paused"),       // 타이머 감소 X
	WaitingInput	UMETA(DisplayName = "WaitingInput"), // 다음 진행을 입력으로 기다림(타이머 감소 X)
	Ended			UMETA(DisplayName = "Ended"),
};

/**
 * EDialogueCommandType
 * - "서버가 마지막으로 반영한 명령"의 종류.
 * - 디버그/재전송/순서 보정(나중) 대비로 남긴다.
 *
 * - 버튼/텍스트/STT 모두 DetectIntent 후 이 타입으로 서버에 요청.
 * - 서버는 Gate(상태검증) 통과 시에만 Accept하고 GameState를 갱신.
 */
UENUM(BlueprintType)
enum class EDialogueCommandType : uint8
{
	None            UMETA(DisplayName = "None"),

	// ----------------------------
	// Dialogue Mode (enter/exit)
	// ----------------------------
	EnterDialogue   UMETA(DisplayName = "EnterDialogue"),
	ExitDialogue    UMETA(DisplayName = "ExitDialogue"),

	// ----------------------------
	// Line Control
	// ----------------------------
	AdvanceLine     UMETA(DisplayName = "AdvanceLine"),      // = Skip (다음 라인)
	RepeatLine      UMETA(DisplayName = "RepeatLine"),       // = Repeat (현재 라인 재시작)
	RestartDialogue UMETA(DisplayName = "RestartDialogue"),  // = Restart (0번부터)

	// ----------------------------
	// Flow Control
	// ----------------------------
	// Speaking <-> Paused 같은 "FlowState 변화"를 의미
	// - Pause/Resume는 실제로는 SetFlowState의 특정 케이스지만
	//   DetectIntent / Gate / 로그 / 스위치문이 단순해져서 enum으로 분리 추천
	Pause           UMETA(DisplayName = "Pause"),            // FlowState = Paused
	Resume          UMETA(DisplayName = "Resume"),           // FlowState = Speaking
	Exit			UMETA(DisplayName = "Exit"),
	Skip            UMETA(DisplayName = "Skip"),
	Repeat          UMETA(DisplayName = "Repeat"),
	Restart         UMETA(DisplayName = "Restart"),
	
	// (옵션) 내부/확장용: "임의 FlowState로 바꾼다"
	// - 이미 네 코드가 SetFlowState 요청 기반이면 유지하는 게 호환에 유리
	SetFlowState    UMETA(DisplayName = "SetFlowState"),
};


/**
 * EDialogueSubState
 * - 대화 모드 안에서의 "연출/카메라/UI" 분기용 세부 상태.
 * - Listening / Speaking 정도만으로도 충분히 강력하다.
 * - FlowState(머신)와 SubState(연출)는 역할이 다르다.
 *
 * 예)
 * - FlowState=Paused 이더라도 SubState는 Listening(유저 입력 대기 연출)일 수 있다.
 */
UENUM(BlueprintType)
enum class EDialogueSubState : uint8
{
	None		UMETA(DisplayName = "None"),
	Listening	UMETA(DisplayName = "Listening"),
	Speaking	UMETA(DisplayName = "Speaking"),
};


/**
 * FMosesDialogueLine
 * - "로컬 데이터(에셋)" 단위의 대사 라인.
 * - LineIndex로 Resolve해서 Subtitle/Duration/VoiceSound를 가져온다.
 *
 * Day10:
 * - SubtitleText 라인과 동일한 VoiceSound를 재생한다.
 */
USTRUCT(BlueprintType)
struct FMosesDialogueLine
{
	GENERATED_BODY()

public:
	/** 말풍선 자막(라인 텍스트) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SubtitleText;

	/** 라인 기본 재생 시간(서버 타이머 기준). 0 이상 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Duration = 2.0f;

	/** 라인별 보이스 사운드(없을 수 있음). Day10 MVP 핵심 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> VoiceSound = nullptr;
};

/**
 * FDialogueNetState
 * - "대화 진행 상태" 전광판.
 * - 서버가 유일하게 수정하고(GameMode/GameState),
 *   클라는 복제된 값을 보고 "표시/연출"만 한다.
 *
 * 중요:
 * - 이 구조 덕분에 LateJoin(늦게 들어온 사람)도
 *   현재 LineIndex/SubState/RemainingTime을 즉시 받으면 화면이 복구된다.
 *
 * 권장 규칙:
 * - GameState의 RepNotify(OnRep)에서는 "브로드캐스트만".
 * - 실제 UI/연출(위젯/사운드/카메라)은 LPS/Widget에서 처리.
 */
USTRUCT(BlueprintType)
struct FDialogueNetState
{
	GENERATED_BODY()

public:
	/** 연출 분기용 서브상태 (서버 확정) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EDialogueSubState SubState = EDialogueSubState::None;

	/** 서버 머신 상태 (Speaking일 때만 RemainingTime 감소) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EDialogueFlowState FlowState = EDialogueFlowState::WaitingInput;

	/** 현재 대사 줄 번호(인덱스). 서버가 다음 줄을 결정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LineIndex = 0;

	/** 현재 라인/상태에서 남은 시간(서버가 감소시키는 값) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RemainingTime = 0.0f;

	/** 타이머 속도 배율(기본 1.0). Day8/Day9 이후 빠른 재생 등에 사용 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RateScale = 1.0f;

	/** 누가 말하는지 같은 연출 플래그(클라 UI/카메라 분기용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bNPCSpeaking = true;

	/** 마지막으로 서버가 반영한 명령 타입(디버그/동기화 기준점) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EDialogueCommandType LastCommandType = EDialogueCommandType::None;

	/** 마지막 명령 시퀀스(증가값). 중복/순서 문제 추적용 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LastCommandSeq = 0;

public:
	/** 유틸: 상태 리셋(서버에서만 사용 권장) */
	void ResetToDefault()
	{
		SubState = EDialogueSubState::None;
		FlowState = EDialogueFlowState::Paused;
		LineIndex = 0;
		RemainingTime = 0.0f;
		RateScale = 1.0f;
		bNPCSpeaking = true;
		LastCommandType = EDialogueCommandType::None;
		LastCommandSeq = 0;
	}
};