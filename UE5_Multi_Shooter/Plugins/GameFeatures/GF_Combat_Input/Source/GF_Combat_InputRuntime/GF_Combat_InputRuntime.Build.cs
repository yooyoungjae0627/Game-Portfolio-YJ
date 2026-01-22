// Copyright ...

using UnrealBuildTool;

public class GF_Combat_InputRuntime : ModuleRules
{
	public GF_Combat_InputRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",

			// GameFeature
			"GameFeatures",
			"ModularGameplay",

			// Input
			"InputCore",
			"EnhancedInput",
		});
	}
}
