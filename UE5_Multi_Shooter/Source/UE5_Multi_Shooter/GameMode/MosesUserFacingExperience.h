// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"
#include "MosesUserFacingExperience.generated.h"

/**
 * 유저가 UI에서 선택하는 "플레이 항목" 데이터 에셋
 * - 어떤 맵을 열지
 * - 어떤 Experience를 적용할지
 *
 * 역할:
 * - 이 정보들을 TravelURL 옵션으로 변환해
 *   HostSessionSubsystem에 넘길 요청 객체를 생성한다.
 *
 * 주의:
 * - 이 클래스는 로딩/조회/판단을 하지 않는다.
 * - 단순히 "선택 결과를 전달하는 용도"다.
 */

//class UCommonSession_HostSessionRequest;

UCLASS()
class UE5_MULTI_SHOOTER_API UMosesUserFacingExperience : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	///**
	// * HostSessionSubsystem에 넘길 요청 객체 생성
	// * - MapID 설정
	// * - Experience 이름을 TravelURL 옵션으로 추가
	// */
	//UFUNCTION(BlueprintCallable)
	//UCommonSession_HostSessionRequest* CreateHostingRequest() const;

	/** 열고 싶은 맵 (PrimaryAsset: Map) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Experience, meta = (AllowedTypes = "Map"))
	FPrimaryAssetId MapID;

	/** 적용할 Experience (PrimaryAsset: YJExperienceDefinition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Experience, meta = (AllowedTypes = "YJExperienceDefinition"))
	FPrimaryAssetId ExperienceID;
	
	
};
