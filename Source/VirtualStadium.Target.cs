using UnrealBuildTool;
using System.Collections.Generic;

public class VirtualStadiumTarget : TargetRules
{
    public VirtualStadiumTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("VirtualStadium");
    }
}

