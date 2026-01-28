using UnrealBuildTool;

public class GF_Combat_UIRuntime : ModuleRules
{
    public GF_Combat_UIRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",

				// GameFeature
				"GameFeatures",
                "ModularGameplay",

				// CommonUI 타입을 헤더에서 쓸 가능성이 높으므로 Public 권장
				"CommonUI"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "UMG",
                "Slate",
                "SlateCore"
            }
        );
    }
}
