using UnrealBuildTool;
using System.Collections.Generic;

public class VirtualStadiumTarget : TargetRules
{
    public VirtualStadiumTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("VirtualStadium");
    }
}

