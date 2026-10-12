using UnrealBuildTool;

public class VirtualStadium : ModuleRules
{
    public VirtualStadium(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // cesium-native's async headers (used by the tile request retry layer) need C++20 and exceptions.
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;

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

        PrivateDependencyModuleNames.Add("CesiumRuntime");

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new[]
                {
                    "MetaHumanCharacterPalette",
                    "MetaHumanCrowd",
                    "MetaHumanCrowdEditor",
                    "HairStrandsCore",
                    "MeshDescription",
                    "StaticMeshDescription",
                    "SkeletalMeshDescription",
                    "AssetRegistry",
                    "UnrealEd"
                }
            );
        }
    }
}
