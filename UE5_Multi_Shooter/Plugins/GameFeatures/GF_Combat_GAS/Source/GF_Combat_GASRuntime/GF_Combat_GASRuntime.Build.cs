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

			// GameFeatureAction / ActivatingContext
			"GameFeatures",

			// GAS
			"GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",

			// =================================================================
			// ✅ [MOD] Dependency mode (실무형)
			// - GF가 프로젝트 타입/로그/AbilitySet을 직접 참조한다.
			// - AMosesPlayerState / UMosesAbilitySet / MosesLogChannels 사용 가능
			// =================================================================
			"UE5_Multi_Shooter",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        });
    }
}
