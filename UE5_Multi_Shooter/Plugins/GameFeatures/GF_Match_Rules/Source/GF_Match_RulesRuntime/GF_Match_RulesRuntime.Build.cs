using UnrealBuildTool;

public class GF_Match_RulesRuntime : ModuleRules
{
    public GF_Match_RulesRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

			// GameFeatureAction / GameFeatureStateChangeContext
			"GameFeatures"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        });
    }
}
