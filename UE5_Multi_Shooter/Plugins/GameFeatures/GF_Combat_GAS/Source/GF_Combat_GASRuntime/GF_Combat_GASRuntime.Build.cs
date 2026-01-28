using UnrealBuildTool;

public class GF_Combat_GASRuntime : ModuleRules
{
    public GF_Combat_GASRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",

				// GameFeature 런타임 훅
				"GameFeatures",

				// GAS 핵심 3종 모듈 (GameplayAbilities 플러그인이 제공)
				"GameplayAbilities",
                "GameplayTags",
                "GameplayTasks"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
				// 필요 시 추가
			}
        );
    }
}
