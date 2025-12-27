// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GF_Lobby_CodeRuntime : ModuleRules
{
    public GF_Lobby_CodeRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        // PCH 사용 방식
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 보통은 굳이 안 넣어도 되지만, 기본 템플릿 형태 유지
        PublicIncludePaths.AddRange(new string[]
        {
			// (비워도 됨)
		});

        PrivateIncludePaths.AddRange(new string[]
        {
			// (비워도 됨)
		});

        // 이 모듈의 Public 헤더에서 직접 쓰거나, 외부에서 이 모듈을 사용할 때 필요한 의존성
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

			// GameFeatureAction 쓸 거라서 필요
			"GameFeatures",
            "UE5_Multi_Shooter",
			// UUserWidget / CreateWidget / AddToViewport 링크 에러 방지
			"UMG"
        });

        // 이 모듈 내부 cpp에서만 쓰는 의존성
        PrivateDependencyModuleNames.AddRange(new string[]
        {
			// UMG 내부가 Slate를 많이 씀 (UI 쓰면 거의 같이 필요)
			"Slate",
            "SlateCore"
        });

        DynamicallyLoadedModuleNames.AddRange(new string[]
        {
			// (비워도 됨)
		});
    }
}
