using UnrealBuildTool;

public class GF_Combat_UIRuntime : ModuleRules
{
	public GF_Combat_UIRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameFeatures",
			"UMG"
		});
	}
}
