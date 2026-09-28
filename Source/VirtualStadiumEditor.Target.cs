using UnrealBuildTool;
using System.Collections.Generic;

public class VirtualStadiumEditorTarget : TargetRules
{
    public VirtualStadiumEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("VirtualStadium");
    }
}

