#pragma once

#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "MSCharacterCatalog.generated.h"

class AMosesCharacter;

/**
 * FMSCharacterEntry
 *
 * [역할]
 * - 로비: CharacterId 기반으로 선택/프리뷰
 * - 매치: SelectedCharacterId(index) 기반으로 PawnClass를 스폰
 *
 * [중요]
 * - PawnClass는 AMosesCharacter 파생 BP(캐릭터 2종)를 지정해야 한다.
 * - SoftClassPtr를 사용하면 패키지 로딩/참조 의존을 완화할 수 있다.
 */

USTRUCT(BlueprintType)
struct FMSCharacterEntry
{
	GENERATED_BODY()

public:
	/** UI/로그/선택용 식별자 (예: Hero_1, Hero_2) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName CharacterId;

	/** ✅ MatchLevel에서 실제로 스폰할 캐릭터 Pawn BP (AMosesCharacter 파생) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<AMosesCharacter> PawnClass;
};

/**
 * UMSCharacterCatalog (DataAsset)
 */

UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMSCharacterCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/** CharacterId(FName)로 엔트리 조회: 로비 프리뷰/선택에 사용 */
	bool FindById(FName InId, FMSCharacterEntry& OutEntry) const;

	/** SelectedCharacterId(index)로 엔트리 조회: 매치 스폰에 사용 */
	bool FindByIndex(int32 InIndex, FMSCharacterEntry& OutEntry) const;

	const TArray<FMSCharacterEntry>& GetEntries() const { return Entries; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FMSCharacterEntry> Entries;
};
