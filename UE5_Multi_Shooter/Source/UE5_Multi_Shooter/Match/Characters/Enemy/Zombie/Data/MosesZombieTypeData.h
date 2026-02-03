#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MosesZombieTypeData.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * Zombie Type Data (Data-only)
 * - 좀비 타입별 스펙(DataAsset)
 * - 피해 적용은 "SetByCaller GE"로 통일
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMosesZombieTypeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UMosesZombieTypeData();

public:
	/** Max HP (Default 100) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Stats")
	float MaxHP;

	/** Melee Damage Magnitude (Default 10) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Combat")
	float MeleeDamage;

	/**
	 * Damage GameplayEffect (SetByCaller)
	 * - Tag: Data.Damage
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Combat")
	TSubclassOf<UGameplayEffect> DamageGE_SetByCaller;

	/**
	 * Attack Montages (2개 권장)
	 * - 서버가 랜덤 선택 -> Multicast로 재생(코스메틱)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Combat")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	// --------------------------------------------------------------------
	// [DAY10] Headshot Feedback -> 공용 Announcement 팝업(모두에게 보임)
	// --------------------------------------------------------------------

	/** 헤드샷 공용 팝업 텍스트(한글 시작 권장: 네 Announcement 위젯이 영어 제거/한글 시작 필터를 함) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Feedback")
	FText HeadshotAnnouncementText;

	/** 헤드샷 팝업 표시 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Feedback")
	float HeadshotAnnouncementSeconds;
};
