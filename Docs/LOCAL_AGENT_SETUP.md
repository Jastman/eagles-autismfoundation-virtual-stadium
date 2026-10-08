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
  -ExecCmds="Automation RunTests LFF.;Quit" -ModelContextProtocolPort=18000 -unattended -nullrhi -nopause -nosplash -stdout -log

# Package a Mac Development build
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJ" -platform=Mac \
  -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="$PWD/Saved/Packaged" -utf8output
```

### Windows (needs a Windows PC)

For a build-only handoff from the Mac, save all assets first, then run
`python3 Tools/Packaging/prepare_windows_handoff.py`. It creates a new
`Saved/WindowsHandoff/` folder from the current saved working files, including
uncommitted content. The copy disables MCP/editor toolsets and VibeUE, keeps
runtime/content plugins, and excludes Mac binaries, caches, source model imports,
and machine-specific ion credentials. The Mac project is unchanged. Copy the
whole handoff folder to Windows and follow its `WINDOWS_BUILD.md`; `BuildWindows.cmd`
compiles and packages a Development build. Use `--destination <new-folder>` for
another export; existing handoffs are never overwritten. Install Windows Cesium
for Unreal separately and select/save the restricted ion token before packaging.

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
| `import_lff_content.py` | `DT_LFF_GoldenPath` from the JSON; the beds and preview sample in `/Game/LFF/Audio` (`Tools/Audio/make_lff_audio.py`); the voice lines (`import_lff_vo.py`); the 30 ticket frames in `/Game/LFF/UI/Ticket` (`Tools/Pipeline/prepare_ticket.py`); the eagle geometry cache, textures and materials in `/Game/LFF/Eagle` (`Tools/Pipeline/prepare_eagle.py`, Blender); `MI_LFF_FireworkGlow`. Replaces them all; skips a part whose source is missing. | Editor closed: `Tools/Editor/run_headless.sh Tools/Editor/import_lff_content.py "LFF content"` |
| `import_lff_vo.py` | Just the voice lines: `SourceData/Audio/VO/VO_*.wav` → `/Game/LFF/Audio/VO/` (non-looping), and a report of steps whose line is missing. Use it when the recorded voice arrives. | Editor closed: `Tools/Editor/run_headless.sh Tools/Editor/import_lff_vo.py "LFF vo"` |
| `build_lff_standins.py` | The extra `CT_` targets (including `CT_StadiumWide`, which the splash orbits through), the stand-in props and labels, the cue actors (3 gates, the arriving car, the eagle's flight, fireworks), 10 parked cars and 15 crowd stand-ins, through Unreal MCP. Removes its previous run first (tag `LFF_StandIn`), traces heights onto the model and saves the level. Takes 35 s with the editor in front, several minutes behind other windows. | Editor open on `L_LFF_Stadium`: `python3 Tools/Editor/build_lff_standins.py` |
| `refine_sensory_spaces.py` | Explicit trailer photo-wrap binding and an illustrative refinement of the existing sensory room. Preserves room placement, staff, doorway and tour camera. Replaces only its tagged room children; static sensory panels and steady lighting. | First run `python3 Tools/Pipeline/prepare_sav_wrap.py` and `python3 Tools/Pipeline/prepare_sensory_room.py` locally (Pillow/numpy). Then run `runpy.run_path(unreal.Paths.project_dir() + "Tools/Editor/refine_sensory_spaces.py", run_name="__main__")` inside Unreal on `L_LFF_Stadium`, PIE stopped. Run again after a full stand-in rebuild to restore the room materials. |
| `build_wbp_lff_modes.py` | The active scene-first HUD (`Docs/UI_DESIGN.md`): upper-left identity/caption, upper-right controls and responsive chapter grid, mode navigation, Explore places and spatial callouts. Rebuilds ten widget trees in place, checks contracts, applies defaults to nested instances, compiles and saves. Also rebuilds `M_LFF_LevelHighlight` and reimports all seven development rows of `DT_LFF_Amenities`. Reuses `WBP_LFF_Settings`; no longer builds the legacy arc material. | Editor open, PIE stopped, MCP running: `python3 Tools/Editor/build_wbp_lff_modes.py`. For another port: `UNREAL_MCP_URL=http://127.0.0.1:8010/mcp` |
| `build_crowd_poses.py` | Four project-local fan clips in `/Game/LFF/Crowd`: seated idle/clap, standing cheer/clap. Uses the existing sample skeleton and idle, not modified engine assets. Default check mode reports joint geometry; write mode refuses to overwrite existing clips. | Inside Unreal: `runpy.run_path(...)`. To create missing assets, set `os.environ["LFF_POSES_WRITE"]="1"` for that run, then restore the environment value. |
| `configure_stadium_crowd.py` | Configures only the existing bowl spawner: six appearances/outfits, measured tread sidecar, local clips, 48 detailed candidates, a 50 cm camera-clear radius and no cleared forward cone. Preserves occupancy and sensory-room/trailer actors; deliberately saves the stadium map. | After native compilation and pose creation, open `L_LFF_Stadium`, stop PIE and run `runpy.run_path(unreal.Paths.project_dir() + "Tools/Editor/configure_stadium_crowd.py", run_name="__main__")`. Do not run the full stand-in builder for this operation. |
| `build_m_lff_timeline_arc.py` | **Legacy presentation only.** `M_LFF_TimelineArc`: Custom node from `SourceData/UI/Materials/LFF_TimelineArc.hlsl`, RGB to Final Color and A to Opacity. Fails if a pin is unconnected. | Inside the editor: `runpy.run_path(...)` through `execute_python_code`; not called by the active HUD builder. |
| `build_m_lff_level_highlight.py` | `M_LFF_LevelHighlight`: Free explore's level highlight, a post-process material (after tonemapping) from `SourceData/UI/Materials/LFF_LevelHighlight.hlsl`. It tints the picked level's height band inside the stadium's footprint, from depth, so the Nanite stadium materials are untouched. | Inside the editor: `runpy.run_path(...)`; called by `build_wbp_lff_modes.py`. |
| `build_wbp_lff_ui.py` | **Superseded for the HUD** (needs `--legacy`; rebuilding `WBP_LFF_HUD` with it would break the new bindings). The old five HUD panels and `WBP_LFF_HUD`, through Unreal MCP. It checks each widget's contract names, compiles and saves. `--rebuild` clears and rebuilds existing widget trees in place (the assets are kept). `--extend` only adds the session 3 widgets (Voice, Crowd, timeline, ticket phone, Learn More) to existing trees. | Editor open with the MCP server running: `python3 Tools/Editor/build_wbp_lff_ui.py` |

Command for `build_lff_stadium_level.py`:

```bash
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -ExecutePythonScript="$PWD/Tools/Editor/build_lff_stadium_level.py" -ModelContextProtocolPort=18000 -unattended -nullrhi -nopause -nosplash -stdout -log
```

### Measured crowd floors

`seats.json` contains chair undersides, not tread heights. If the source seats or structure chunks change, regenerate the aligned sidecar in Blender before configuring the spawner:

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
  --python Tools/Pipeline/build_seat_treads.py -- \
  --chunks Saved/StadiumImport/v1 --seats Content/LFF/Crowd/seats.json \
  --out Content/LFF/Crowd/seat_treads.json
```

The current sidecar has 56,643 measured seat rows and 64 explicit unmeasured rows. Runtime validates the exact seat-file hash and foot offsets, reports and excludes unmeasured seats, and rejects invalid/mismatched data. Do not substitute a guessed height. Standing feet are 70 cm forward; seated feet are 45 cm forward and 13 cm lateral. `Config/DefaultGame.ini` already stages the crowd JSON directory and cooks the animation assets.

Crowd-only recovery order: preserve the saved map/assets, compile the native editor target normally, generate missing clips, run `configure_stadium_crowd.py`, then verify My seat, Crowd Off, Calm Mode and Less movement in PIE. A full `build_lff_standins.py` run replaces stand-ins and requires reapplying sensory refinements; it is not the crowd configuration command.

Sensory-room refinements (`SR_`) must remain Movable and attached to `SI_SensoryRoomProxy`, with relative location, rotation and scale. Setting their mesh components to Static detaches them from the movable room root and can leave room-local offsets at midfield. `refine_sensory_spaces.py` maintains compatible mobility and verifies these attachments before saving; moving the room root then carries its furnishings with it.

Things to know:
- **Exit code 1 is expected from the headless level build.** On 5.8.3 the editor crashes *after* the script has saved, during Slate text teardown in `quit_editor` (`UserActivityHint=EditorExit`). Check for the `[LFF build] DONE` and `validate OK` log lines instead. They're in the crash folder's `VirtualStadium.log` under `~/Library/Application Support/Epic/UnrealEngine/5.8/Saved/Crashes/` when `Saved/Logs` doesn't get them.
- **Cesium needs the `DEFAULT_GEOREFERENCE` tag** on the level's georeference. Without it, Cesium spawns a second georeference at its Denver demo origin, and the tileset and anchors resolve to that one. The script tags it and fails if there isn't exactly one.
- **MCP gotchas:**
  - `TextRenderComponent.TextRenderColor` takes **0–1** channels over MCP (`{"r": 0.47, ...}`); 0–255 values clamp to white.
  - `add_to_scene_from_class` with a `parent` takes the transform **relative to that parent**.
  - `ObjectTools.set_properties` takes `values` as a **JSON-encoded string**. An object silently returns `false`.
  - `UMGToolSet.GetWidgets` lists the parent's unbound `BindWidget` names as placeholders with `widget: "None"`. Ignore those when you check what exists.
  - The UMG toolset can't create a level or run console commands, which is why the level build is a headless Python script.
- **The MCP server auto-starts** because `bAutoStartServer=True` is set in `Saved/Config/MacEditor/EditorPerProjectUserSettings.ini`. That file is per-user and not committed.
- **Headless runs must use `-ModelContextProtocolPort=18000`** (as in the commands above). `bAutoStartServer` is per-project, so every headless editor also starts an MCP server. When it crashes on exit, its `CrashReportClient` **inherits the listening socket** and holds the port without answering. On 8000 that blocks the interactive editor's server: `HttpListener unable to bind` in the log, or MCP calls that hang. The plugin has no switch to turn the server off, so move it to a spare port. To clear a stuck port, `lsof -nP -iTCP:8000` and stop the orphaned `CrashReportClient`.
- **Wait about 30 s between closing and reopening the editor.** Otherwise the new editor can't bind `127.0.0.1:8000` (`LogHttpListener: Error: HttpListener unable to bind`), because the old socket is still being released. The server doesn't retry. If that happens, run `ModelContextProtocol.StartServer` in the editor console.
- **`CaptureViewport` shows black where Cesium hasn't streamed tiles.** Cesium picks tiles for the *real* editor viewport's frustum, and a capture with a different aspect ratio can include regions that were never loaded. Judge coverage in the viewport or PIE, not from a capture's edges. The capture also needs explicit `captureTransform` and `annotations` arguments, and a camera sitting on an `LFFCameraTarget` sees its editor-only arrow, so offset about 1.5 m forward.
- **Line traces work on the tiles** (`SceneTools.trace_world`), because Cesium builds collision. Batch them in `ProgrammaticToolset.execute_tool_script` for profiles (hundreds of traces take seconds). Traces return `None` where no tile with collision is loaded.
- **The Cesium ion token** lives in `DefaultIonAccessToken` on `Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset`. That folder is **git-ignored** so the token can't be committed. On each machine, choose the token in the Cesium panel → **Token** (use the restricted demo token, D15). Cesium recreates the asset if it's missing. After choosing the token, **save** `CesiumIonSaaS` (it stays dirty in memory otherwise) and refresh the tileset.

## 6. Stadium model pipeline (ADR-0004)

Run these in order. Everything they read lives in `Assets/` and `Saved/StadiumImport/`, and neither folder is committed.

1. **Sample the tile heights.** With the editor open and the tiles visible, before the site is clipped:

   ```bash
   python3 Tools/Editor/sample_tile_dsm.py
   ```

   It writes `Saved/StadiumImport/tile_dsm.json`. You only need to rerun it if the georeference changes.
2. **Prepare the model in Blender** (about 25 min, mostly OBJ import of the seats):

   ```bash
   /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/prepare_stadium.py -- --src "Assets/Eagles Stadium Philadelphia 3D Model/OBJ File format (Exchange)" --tile-dsm Saved/StadiumImport/tile_dsm.json --out Saved/StadiumImport/v1
   ```

3. **Import into Unreal and swap it into the level** (editor closed; about 30 min, mostly Nanite builds):

   ```bash
   Tools/Editor/run_headless.sh Tools/Editor/import_stadium.py "LFF stadium"
   ```

4. **Apply the cinematic look** (editor closed):

   ```bash
   Tools/Editor/run_headless.sh Tools/Editor/setup_cinematic_look.py "LFF look"
   ```

5. **Preview framing without the editor:**

   ```bash
   /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/probe_views.py -- --chunks Saved/StadiumImport/v1 --out <dir> --views views.json
   ```

Notes:
- `run_headless.sh` exists because 5.8.3 editors hang or crash in `quit_editor` after these scripts. It stops that exact process once the script logs DONE or FAILED. Headless logs go to `~/Library/Logs/Unreal Engine/VirtualStadiumEditor/`, not to the redirected stdout.
- **Tiles don't stream while the editor is in the background** unless `bThrottleCPUWhenNotForeground=False` is set. That setting is per user (`Saved/Config/MacEditor/EditorPerProjectUserSettings.ini`, under `[/Script/UnrealEd.EditorPerformanceSettings]`). Without it, Cesium gets no camera: tile load progress sits at 0 and nothing is logged.
- **The standalone game logs** to `~/Library/Logs/VirtualStadium/`.
