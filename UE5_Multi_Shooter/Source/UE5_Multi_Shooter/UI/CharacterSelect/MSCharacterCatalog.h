// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MSCharacterCatalog.generated.h"

/**
 * 캐릭터 선택용 데이터 1개
 * - "선택 ID"는 서버에 보내는 최소 단위(서버 권한 확정 키)
 * - 프리뷰는 SkeletalMesh만 있어도 충분 (추후 AnimBP/아이들/스킨 확장 가능)
 */
USTRUCT(BlueprintType)
struct FMSCharacterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USkeletalMesh> PreviewMesh = nullptr;
};

/**
 * Character Select에서 사용할 "캐릭터 목록" 소스
 * - WBP/Widget은 이 Catalog를 읽어서 좌/우 인덱스로 돌린다.
 */
UCLASS(BlueprintType)
class UE5_MULTI_SHOOTER_API UMSCharacterCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FMSCharacterEntry>& GetEntries() const { return Entries; }
	bool FindById(const FName InId, FMSCharacterEntry& OutEntry) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FMSCharacterEntry> Entries;
};