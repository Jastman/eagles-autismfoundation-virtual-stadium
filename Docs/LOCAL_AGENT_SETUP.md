# Local agent setup: UE 5.8, Unreal MCP and the Unreal CLI

Status: Draft, 2026-09-28. The engine-side details come from web search summaries. Epic's and Cesium's official doc sites were blocked from the cloud agent, so **verify every item marked (VERIFY) on the engine workstation.**

## Why this is needed

The cloud Claude Code session that wrote this repo has no Unreal Engine, no GPU and no Windows. Unreal MCP runs **inside the Unreal Editor process** and listens on `localhost` (reported as `http://127.0.0.1:8000/mcp`, VERIFY). A cloud container can't reach a `localhost` server on your workstation.

**The work that needs the editor must run in Claude Code on the engine workstation.** That covers compiling, running in the editor, creating assets, importing, aligning and packaging. Use the Claude Code CLI or the desktop app there, pointed at a local clone of this repo. The cloud session can keep producing code, config and docs on the same branch.

Don't expose the editor's MCP port to the internet with a tunnel just so the cloud session can reach it. It gives remote control of the editor, including arbitrary Python and file access.

## 1. Engine and plugins

| Item | Value | Status |
|---|---|---|
| Unreal Engine | 5.8.3 (installed on the workstation per user) | User-stated |
| Cesium for Unreal | Latest public release from Fab (https://www.fab.com/listings/76c295fe-0dc6-4fd6-8319-e9833be427cd); v2.28.0 was reported as the first with 5.8 support | Install via Fab → Epic Launcher → "Install to Engine" 5.8. **Record the exact version in LEDGER.** |
| Unreal MCP plugin | Ships with UE 5.8, **Experimental** | VERIFY plugin name in Edit → Plugins |
| Companion plugins | "Terminal" and "Editor Tool Set" (reported as required alongside Unreal MCP) | VERIFY |
| Source build needed? | Sources disagree: some say it's included in the Launcher build, one says it needs a source build | **VERIFY first**; this decides the workstation setup |

Enable the plugins **through Edit → Plugins in the editor**. Don't hand-edit plugin names into `VirtualStadium.uproject`. The editor writes the correct internal names, and those `.uproject` changes should then be committed.

## 2. Connect Claude Code to Unreal MCP

1. Open `VirtualStadium.uproject` in UE 5.8 with the plugins above enabled, then restart the editor.
2. In the editor console, run the client-config generator. It was reported as `ModelContextProtocol.GenerateClientConfig ClaudeCode` (VERIFY; if the console lists a different name, use that).
3. Inspect the generated config before using it. If it contains no secrets or machine-specific absolute paths, commit it as the project `.mcp.json`. Otherwise add it to `.gitignore` and record the shape of it here.
4. Start Claude Code in the repo root on the engine workstation and run `/mcp`. The Unreal server should show as connected.
5. Smoke test: ask the agent to list the actors in the current level. Record the result in `Docs/LEDGER.md` as evidence.

Security:
- Keep the MCP server bound to `127.0.0.1` only.
- Review the tools the server exposes (Python execution, file access and similar) before approving them in Claude Code.
- Don't auto-approve destructive tools such as delete asset, overwrite or save-all without review.

## 3. Unreal CLI (repeatable build evidence)

These are the standard UE entry points. The install paths are the Launcher defaults; adjust them if yours differ. Flags are **VERIFY** on 5.8.3.
- **Unreal can't cross-compile Mac from Windows or Windows from Mac**, so each platform's package has to be built on that OS.
- Save each log under `Saved/Logs/` (git-ignored). Paste the pass/fail result and the first error into `Docs/LEDGER.md` §9.

### macOS (current workstation)
```bash
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
PROJ="$PWD/VirtualStadium.uproject"

# Compile the editor target
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" VirtualStadiumEditor Mac Development -Project="$PROJ" -WaitMutex

# Headless tests (LFF.*)
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" \
  -ExecCmds="Automation RunTests LFF.;Quit" -unattended -nullrhi -nopause -nosplash -stdout -log

# Package a Mac Development build
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJ" -platform=Mac \
  -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="$PWD/Saved/Packaged" -utf8output
```

### Windows (needs a Windows PC)
```bat
set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
set PROJ=%CD%\VirtualStadium.uproject

"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" VirtualStadiumEditor Win64 Development -Project="%PROJ%" -WaitMutex
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -ExecCmds="Automation RunTests LFF.;Quit" -unattended -nullrhi -nopause -log
"%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%CD%\Saved\Packaged" -utf8output
```

### Releasing (GitHub Releases, private repo)
- Zip each platform's archive folder: `LFF-VirtualGameDay-<version>-Mac.zip` and `-Win64.zip`. Each file must be under 2 GiB.
- Upload both through the GitHub web UI, or with `gh release create` on the workstation.
- **Mac.** An unsigned app is blocked by Gatekeeper. There are two options:
  - (a) Sign and notarize it with an Apple Developer ID. This is best.
  - (b) For collaborators only, the release notes tell them to run `xattr -dr com.apple.quarantine "<App>.app"`, or to right-click → Open.
- **Windows.** An unsigned exe shows SmartScreen. The release notes say "More info → Run anyway".
- Collaborators need internet access while running it, because the Google Photorealistic 3D Tiles stream live and can't be cached.

## 4. Known 5.4 → 5.8 migration watch items (from the existing scaffold)

The first local compile will show which of these matter.
- `Source/VirtualStadium/*.Target.cs` uses `BuildSettingsVersion.V5`. UBT may warn on 5.8. Change it only if the build asks.
- `Config/DefaultInput.ini` uses legacy `ActionMappings`. The architecture phase will decide whether to move to Enhanced Input.
- `VirtualStadium.Build.cs` has no `CesiumRuntime` dependency. Add it only when C++ actually calls Cesium APIs.
