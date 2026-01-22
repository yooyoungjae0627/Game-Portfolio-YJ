// Copyright ...

using UnrealBuildTool;

public class GF_Combat_GASRuntime : ModuleRules
{
	public GF_Combat_GASRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",

			// GameFeature 기본
			"GameFeatures",
			"ModularGameplay",

			// GAS를 여기서 “진짜로” 쓸 거면 유지
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
