#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "MSCharacterCatalog.generated.h"

class APawn;

/**
 * FMSCharacterEntry
 *
 * [역할]
 * - 로비: CharacterId 기반으로 선택/프리뷰
 * - 매치: SelectedCharacterId(1-base) 기반으로 PawnClass를 스폰
 *
 * [DAY2/구조 원칙]
 * - Catalog는 "PawnClass"만 안다. (AMosesCharacter에 의존하지 않는다)
 * - GameMode/Experience가 특정 캐릭터 구현체에 강결합되지 않게 한다.
 *
 * [주의]
 * - 실제로는 AMosesCharacter 파생 BP를 넣어도 된다.
 *   다만 타입은 APawn으로 둬서 의존성을 끊는다.
 */
USTRUCT(BlueprintType)
struct FMSCharacterEntry
{
	GENERATED_BODY()

public:
	/** UI/로그/선택용 식별자 (예: Hero_01, Hero_02) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName CharacterId;

	/** MatchLevel에서 실제로 스폰할 Pawn BP (대부분 AMosesCharacter 파생 BP가 들어오게 됨) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<APawn> PawnClass;
};

/**
 * UMSCharacterCatalog (DataAsset)
 *
 * [역할]
 * - 캐릭터 선택 -> PawnClass 매핑 데이터
 *
 * [정책]
 * - SelectedCharacterId는 1-base(1..N)
 * - Entries 배열은 0-base
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMSCharacterCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/** CharacterId(FName)로 엔트리 조회: 로비 프리뷰/선택에 사용 */
	bool FindById(FName InId, FMSCharacterEntry& OutEntry) const;

	/** SelectedCharacterId(1-base)로 엔트리 조회: 매치 스폰에 사용 */
	bool FindByIndex(int32 InSelectedId, FMSCharacterEntry& OutEntry) const;

	const TArray<FMSCharacterEntry>& GetEntries() const { return Entries; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FMSCharacterEntry> Entries;
};
