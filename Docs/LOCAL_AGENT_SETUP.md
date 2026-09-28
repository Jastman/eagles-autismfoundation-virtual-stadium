# Local agent setup: UE 5.8, Unreal MCP and the Unreal CLI

Status: Draft, 2026-09-28. The engine-side details in §1 and §2 were **verified on the Mac workstation on 2026-09-28** by reading the installed plugin descriptors and the Unreal MCP plugin source (UE 5.8.3 Launcher build, CL 58210709). Items still marked (VERIFY) haven't been checked yet.

## Why this is needed

The cloud Claude Code session that wrote this repo has no Unreal Engine, no GPU and no Windows. Unreal MCP runs **inside the Unreal Editor process** and listens on `localhost` (reported as `http://127.0.0.1:8000/mcp`, VERIFY). A cloud container can't reach a `localhost` server on your workstation. Verified: the default URL is `http://127.0.0.1:8000/mcp`, and the engine's HTTP server binds to `localhost` by default (`HttpServerConfig.h`).

**The work that needs the editor must run in Claude Code on the engine workstation.** That covers compiling, running in the editor, creating assets, importing, aligning and packaging. Use the Claude Code CLI or the desktop app there, pointed at a local clone of this repo. The cloud session can keep producing code, config and docs on the same branch.

Don't expose the editor's MCP port to the internet with a tunnel just so the cloud session can reach it. It gives remote control of the editor, including arbitrary Python and file access.

## 1. Engine and plugins

| Item | Value | Status |
|---|---|---|
| Unreal Engine | 5.8.3, Launcher build, CL 58210709, at `/Users/Shared/Epic Games/UE_5.8` | Verified (`Engine/Build/Build.version`) |
| Cesium for Unreal | **2.29.1** (descriptor version 95, `EngineVersion` 5.8.0), installed to the engine under `Engine/Plugins/Marketplace/` | Verified (`CesiumForUnreal.uplugin`) |
| Unreal MCP | Plugin **"Unreal MCP"** (internal name `ModelContextProtocol`), Experimental, ships with the Launcher build under `Engine/Plugins/Experimental/`. Depends on `ToolsetRegistry`, which pulls in `PythonScriptPlugin`, `EditorScriptingUtilities` and `FileSandbox`. | Verified |
| Companion plugins | **"Terminal"** (`Terminal`, Slate terminal emulator). The "Editor Tool Set" is **"EditorToolset"** (`EditorToolset`: actors, Blueprints and properties). The Unreal MCP plugin exposes only the tools that the enabled `*Toolset` plugins register. | Verified |
| Recommended toolsets | `EditorToolset`, `UMGToolSet` (for `WBP_LFF_HUD`) and `AutomationTestToolset` (runs `LFF.*` tests). Don't enable `AllToolsets`: it pulls in 21 toolsets (GAS, MetaHuman, Niagara and more) and widens what the agent can touch. | Recommendation |
| Source build needed? | **No.** Everything above ships in the Launcher build. | Verified |

Enable the plugins **through Edit → Plugins in the editor**. Don't hand-edit plugin names into `VirtualStadium.uproject`. The editor writes the correct internal names, and those `.uproject` changes should then be committed.

## 2. Connect Claude Code to Unreal MCP

1. Open `VirtualStadium.uproject` in UE 5.8 and enable **Unreal MCP**, **Terminal**, **EditorToolset**, **UMG ToolSet** and **AutomationTestToolset** in Edit → Plugins. Restart the editor.
2. The server **doesn't auto-start** (`bAutoStartServer` defaults to false). Either run `ModelContextProtocol.StartServer` in the editor console each session, or tick **Auto Start Server** in Editor Preferences → Model Context Protocol. That setting is per-user (`EditorPerProjectUserSettings`, under `Saved/`), so it isn't committed.
3. In the editor console, run `ModelContextProtocol.GenerateClientConfig ClaudeCode` (verified; the other clients are `Cursor|VSCode|Gemini|Codex|All`). On a Launcher build it writes `.mcp.json` to the project root, using the `mcpServers` key and a `url` of `http://127.0.0.1:<port><path>`.
4. Inspect the generated config before using it. If it contains no secrets or machine-specific absolute paths, commit it as the project `.mcp.json`. Otherwise add it to `.gitignore` and record the shape of it here. **Verified 2026-09-28:** it contains one entry, `"unreal-mcp": {"type": "http", "url": "http://127.0.0.1:8000/mcp"}`, with no secrets or paths, so it's committed.
5. Start Claude Code in the repo root on the engine workstation and run `/mcp`. The Unreal server should show as connected. By default (`bEnableToolSearch`), the server lists only `list_toolsets`, `describe_toolset` and `call_tool`, and the agent discovers the toolset tools on demand.
6. Smoke test: ask the agent to list the actors in the current level. Record the result in `Docs/LEDGER.md` as evidence.

**Calling convention (verified).** Toolset tools go through `call_tool` with `{"tool_name": "<short name>", "toolset_name": "<toolset>", "arguments": {...}}`. Use the short tool name (`find_actors`), not the fully qualified one. For example, `call_tool` with `tool_name` `find_actors` and `toolset_name` `editor_toolset.toolsets.scene.SceneTools` lists the actors in the level. `find_actors` rejects `{}`, so pass empty `name`, `tag` and `collision_channels`.

Other console commands: `ModelContextProtocol.StopServer` and `ModelContextProtocol.RefreshTools`. The port and path are set in the same settings page (defaults `8000` and `/mcp`).

Security:
- Keep the MCP server bound to `127.0.0.1` only.
- Review the tools the server exposes (Python execution, file access and similar) before approving them in Claude Code.
- Don't auto-approve destructive tools such as delete asset, overwrite or save-all without review.
- **With tool search on (the default), every toolset tool runs through the single `call_tool` MCP tool.** "Always allow" on `call_tool` therefore approves *all* of them, including `remove_from_scene`, `save_actor`, asset/file tools and the sandboxed Python `ProgrammaticToolset`. Approve `call_tool` per call. It's safe to always-allow `list_toolsets` and `describe_toolset`, which are read-only.

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
- **Resolved 2026-09-28.** The `*.Target.cs` files were in `Source/VirtualStadium/`. UBT looks for them at the `Source/` root, so it treated the project as having no code, generated its own temporary targets, and failed with duplicate type errors. They're now at `Source/VirtualStadium.Target.cs` and `Source/VirtualStadiumEditor.Target.cs`.
- **Resolved 2026-09-28.** `BuildSettingsVersion.V5` failed on 5.8.3, because the editor target shares build products with the Launcher `UnrealEditor` and V5 changes four warning levels. Both targets now use `BuildSettingsVersion.Latest`, and the build compiles with no warnings.
- `Config/DefaultInput.ini` uses legacy `ActionMappings`. The architecture phase will decide whether to move to Enhanced Input.
- `VirtualStadium.Build.cs` has no `CesiumRuntime` dependency. Add it only when C++ actually calls Cesium APIs.

## 5. Editor build scripts (`Tools/Editor/`, verified 2026-09-28 on the Mac)

These scripts do ManualSetup §2–§5 reproducibly. Each one refuses to overwrite its asset, so hand-framed shots and hand-edited widgets are safe. To rebuild, delete the asset first.

| Script | Builds | How to run |
|---|---|---|
| `build_lff_stadium_level.py` | `DT_LFF_GoldenPath` (imported from the JSON) and `L_LFF_Stadium`: the georeference, Google tileset (ion asset 2275207), sun, `LFF_SiteRoot` with its globe anchor, the 4 `CT_` targets, the labels and the proxies. It validates the structure before saving. | **Close the editor first**, then run the command below this table. |
| `build_wbp_lff_hud.py` | `WBP_LFF_HUD`, through Unreal MCP. It checks that all 30 contract widget names exist, compiles and saves. | Editor open with the MCP server running: `python3 Tools/Editor/build_wbp_lff_hud.py` |

Command for `build_lff_stadium_level.py`:

```bash
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -ExecutePythonScript="$PWD/Tools/Editor/build_lff_stadium_level.py" -unattended -nullrhi -nopause -nosplash -stdout -log
```

Things to know:
- **Exit code 1 is expected from the headless level build.** On 5.8.3 the editor crashes *after* the script has saved, during Slate text teardown in `quit_editor` (`UserActivityHint=EditorExit`). Check for the `[LFF build] DONE` and `validate OK` log lines instead. They're in the crash folder's `VirtualStadium.log` under `~/Library/Application Support/Epic/UnrealEngine/5.8/Saved/Crashes/` when `Saved/Logs` doesn't get them.
- **Cesium needs the `DEFAULT_GEOREFERENCE` tag** on the level's georeference. Without it, Cesium spawns a second georeference at its Denver demo origin, and the tileset and anchors resolve to that one. The script tags it and fails if there isn't exactly one.
- **MCP gotchas:**
  - `ObjectTools.set_properties` takes `values` as a **JSON-encoded string**. An object silently returns `false`.
  - `UMGToolSet.GetWidgets` lists the parent's unbound `BindWidget` names as placeholders with `widget: "None"`. Ignore those when you check what exists.
  - The UMG toolset can't create a level or run console commands, which is why the level build is a headless Python script.
- **The MCP server auto-starts** because `bAutoStartServer=True` is set in `Saved/Config/MacEditor/EditorPerProjectUserSettings.ini`. That file is per-user and not committed.
- **The Cesium ion token** lives in `DefaultIonAccessToken` on `Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset`. That folder is **git-ignored** so the token can't be committed. On each machine, choose the token in the Cesium panel → **Token** (use the restricted demo token, D15). Cesium recreates the asset if it's missing.

