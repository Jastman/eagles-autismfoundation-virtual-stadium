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

These are standard UE batch entry points. Paths assume a Windows Launcher install; adjust `UE_ROOT` for a source build. Check the flags against the 5.8 docs (VERIFY).

```bat
set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
set PROJ=%CD%\VirtualStadium.uproject

:: Compile the editor target (Gate 1 evidence: C++ builds on 5.8)
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" VirtualStadiumEditor Win64 Development -Project="%PROJ%" -WaitMutex

:: Package a Win64 Development build (Gate 6 evidence)
"%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%CD%\Saved\Packaged" -utf8output
```

Save each build log under `Saved/Logs/` and paste the result (pass/fail plus the first error) into `Docs/LEDGER.md`. `Saved/` is git-ignored.

## 4. Known 5.4 → 5.8 migration watch items (from the existing scaffold)

The first local compile will show which of these matter.
- `Source/VirtualStadium/*.Target.cs` uses `BuildSettingsVersion.V5`. UBT may warn on 5.8. Change it only if the build asks.
- `Config/DefaultInput.ini` uses legacy `ActionMappings`. The architecture phase will decide whether to move to Enhanced Input.
- `VirtualStadium.Build.cs` has no `CesiumRuntime` dependency. Add it only when C++ actually calls Cesium APIs.
