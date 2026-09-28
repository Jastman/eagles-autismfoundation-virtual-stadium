using UnrealBuildTool;
using System.Collections.Generic;

public class VirtualStadiumEditorTarget : TargetRules
{
    public VirtualStadiumEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("VirtualStadium");
    }
}

