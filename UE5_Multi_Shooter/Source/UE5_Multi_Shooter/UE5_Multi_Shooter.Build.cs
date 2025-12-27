// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE5_Multi_Shooter : ModuleRules
{
	public UE5_Multi_Shooter(ReadOnlyTargetRules Target) : base(Target)
	{
        // PCH(미리 컴파일된 헤더) 사용 방식
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 다른 모듈에서도 접근 가능한 공개 의존성
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",            // 기본 타입, 로그
            "CoreUObject",     // UObject, 리플렉션
            "Engine",          // Actor, World
            "InputCore",       // 기본 입력
            "EnhancedInput",   // Enhanced Input 시스템

            "GameplayTags",    // GAS 태그 시스템
            "ModularGameplay", // GameFeature 기반 구조
            "GameFeatures",    // GameFeature 플러그인
            "Projects",        // PluginManager 해결
            "NetCore",
            "UMG",
            "CommonUI",   
        });

        // 필요하면 Private 쪽에도 가능
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
               // "ModularGameplay" // 필요 시
        });
    }
}
