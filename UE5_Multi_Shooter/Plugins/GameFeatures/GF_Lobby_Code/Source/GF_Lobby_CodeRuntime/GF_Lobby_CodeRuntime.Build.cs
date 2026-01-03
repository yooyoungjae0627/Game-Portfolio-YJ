using UnrealBuildTool;

public class GF_Lobby_CodeRuntime : ModuleRules
{
    public GF_Lobby_CodeRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            "GameFeatures",
            "UMG",
            "UE5_Multi_Shooter" // ★ 이거 추가
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
        });
    }
}
