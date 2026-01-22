// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE5_Multi_Shooter : ModuleRules
{
    public UE5_Multi_Shooter(ReadOnlyTargetRules Target) : base(Target)
    {
        // PCH 사용 방식
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // ============================
        // Public Dependencies
        // ============================
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            "InputCore",
            "EnhancedInput",

            "ModularGameplay",
            "GameFeatures",

            "Projects",
            "NetCore",

            "UMG",
            "CommonUI",

            "GameplayAbilities",
            "GameplayTasks",
            "GameplayTags"
        });

        // ============================
        // Private Dependencies
        // ============================
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
        });

        // ============================
        // Platform-specific
        // ============================
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Voice / WinRT / Azure Speech 미사용
            // → 아무 것도 추가하지 않음
        }
    }
}
