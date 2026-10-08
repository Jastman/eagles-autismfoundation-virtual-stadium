"""Export saved project files for a native Windows build, without editor tooling."""

import argparse
import json
from pathlib import Path
import re
import shutil


EDITOR_PLUGINS = {
    "ModelContextProtocol",
    "Terminal",
    "EditorToolset",
    "UMGToolSet",
    "AutomationTestToolset",
    "VibeUE",
}

BUILD_COMMAND = r"""@echo off
setlocal
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJ=%~dp0VirtualStadium.uproject"
if not exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo ERROR: Unreal Engine not found. Set UE_ROOT to your UE 5.8.3 installation.
    exit /b 1
)
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" VirtualStadiumEditor Win64 Development -Project="%PROJ%" -WaitMutex
if errorlevel 1 exit /b 1
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%~dp0Saved\Packaged" -prereqs -utf8output
if errorlevel 1 exit /b 1
echo Windows package created under "%~dp0Saved\Packaged".
"""

HANDOFF_README = r"""# Windows build handoff

This is a snapshot of saved files, including uncommitted and untracked assets.
It is not a packaged application and has not been compiled on Windows.

## Required on the Windows PC

- Unreal Engine 5.8.3.
- Visual Studio 2022 17.14+ with Game development with C++, the UE-compatible
  MSVC toolchain, and Windows SDK (or another toolchain supported by UE 5.8).
- The Windows version of Cesium for Unreal, preferably 2.29.1 to match the Mac,
  installed into this engine. Keep the enabled engine runtime/content plugins:
  MetaHumanCrowdContent, MassGameplay, and ChaosClothAsset.
- Internet access for streamed Google Photorealistic 3D Tiles.

MCP, Terminal, editor toolsets, and VibeUE are disabled only in this copy.
No Mac binaries, build caches, original model sources, or editor scripts are needed.
SourceData/Journey is included for the C++ editor tests.

## Build

1. Copy this entire folder to a local path, such as C:\\Projects\\VirtualStadium.
2. Open Command Prompt in the folder and compile:

   set "UE_ROOT=C:\\Program Files\\Epic Games\\UE_5.8"
   "%UE_ROOT%\\Engine\\Build\\BatchFiles\\Build.bat" VirtualStadiumEditor Win64 Development -Project="%CD%\\VirtualStadium.uproject" -WaitMutex

3. Open VirtualStadium.uproject. The ion token asset was deliberately excluded:
   choose the restricted project token in the Cesium panel and save the recreated
   CesiumIonSaaS asset. Confirm the stadium, tour, Free Explore, audio, and crowd.
4. Close Unreal Editor, then run BuildWindows.cmd from Command Prompt.
   If Unreal is installed elsewhere, set UE_ROOT first.
5. Launch the executable under Saved\\Packaged and check the complete experience.
   Transfer the whole packaged Windows folder, not just the executable.

The first build uses Development for troubleshooting. After validating it, use
-clientconfig=Shipping in BuildWindows.cmd for a Shipping package.
The packaged app still needs internet access and a scoped read-only ion token.
Do not distribute it until asset licensing and attribution requirements are met.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--destination",
        type=Path,
        help="New output folder; defaults to Saved/WindowsHandoff. Never overwrites.",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    destination = (args.destination or root / "Saved/WindowsHandoff").resolve()
    if destination.exists():
        parser.error(f"Destination already exists: {destination}. Use a new folder.")
    for folder in ("Content", "Config", "Source", "SourceData"):
        if destination.is_relative_to(root / folder):
            parser.error(f"Destination must not be inside {folder}.")

    descriptor = json.loads((root / "VirtualStadium.uproject").read_text())
    removed = [
        plugin["Name"]
        for plugin in descriptor.get("Plugins", [])
        if plugin["Name"] in EDITOR_PLUGINS and plugin.get("Enabled", False)
    ]
    for plugin in descriptor.get("Plugins", []):
        if plugin["Name"] in EDITOR_PLUGINS:
            plugin["Enabled"] = False

    required = (
        root / "Config",
        root / "Source",
        root / "Content/LFF/Levels/L_LFF_Stadium.umap",
        root / "Content/LFF/Crowd/seats.json",
        root / "Content/LFF/Crowd/queue.json",
        root / "SourceData/Journey/LFF_GoldenPath.json",
        root / "SourceData/Journey/LFF_Amenities.json",
    )
    for path in required:
        if not path.exists():
            parser.error(f"Required saved project file or folder missing: {path}")

    destination.mkdir(parents=True)
    print(f"Copying saved project files to {destination}", flush=True)

    def ignore_content(directory, names):
        ignored = {".DS_Store", "__pycache__"} & set(names)
        if Path(directory).resolve() == root / "Content/CesiumSettings":
            ignored.add("CesiumIonServers")
        return ignored

    for folder in ("Config", "Content", "Source", "SourceData/Journey"):
        shutil.copytree(root / folder, destination / folder, ignore=ignore_content)

    # The Android editor service is unrelated to Windows and contains a workstation token.
    engine_config = destination / "Config/DefaultEngine.ini"
    text = engine_config.read_text()
    text = re.sub(
        r"(?ms)^\[/Script/AndroidFileServerEditor\.AndroidFileServerRuntimeSettings\]\r?\n.*?(?=^\[|\Z)",
        "",
        text,
    )
    engine_config.write_text(text)
    (destination / "VirtualStadium.uproject").write_text(
        json.dumps(descriptor, indent=4) + "\n"
    )
    (destination / "BuildWindows.cmd").write_bytes(
        BUILD_COMMAND.replace("\n", "\r\n").encode("ascii")
    )
    (destination / "WINDOWS_BUILD.md").write_text(HANDOFF_README)
    print(f"Disabled editor plugins: {', '.join(removed)}")
    print("Kept all saved Content assets except the machine-specific ion token.")
    print("Ready for transfer. Windows compilation and runtime validation are still required.")


if __name__ == "__main__":
    main()
