#pragma once

#include "MosesLobbyViewTypes.generated.h"

/**
 * ELobbyViewMode
 * - 로비 UI/카메라 "연출 모드" (로컬 전용)
 * - Replication 불필요 (규칙 보기 = 게임 규칙이 아니라 화면 모드)
 */
UENUM()
enum class ELobbyViewMode : uint8
{
	Default = 0,
	RulesView,
};
