using UnrealBuildTool;

public class VirtualStadium : ModuleRules
{
    public VirtualStadium(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "EnhancedInput",
                "Slate",
                "SlateCore",
                "UMG",
                "MassEntity",
                "MassSpawner",
                "Json",
                "ChaosClothAssetEngine"
            }
        );
    }
}
