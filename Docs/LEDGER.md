# Project Ledger — LFF Virtual Game-Day Schedule

Living document. Update every cycle. Tags: **VERIFIED** (inspected this repo/session), **ASSUMED** (temporary, dated), **BLOCKED** (needs an input), **UNKNOWN**.

Last updated: 2026-09-28 (Phase 0, cycle 2)

## 1. Facts (verified by inspection, 2026-09-28)

| # | Fact | Source |
|---|---|---|
| F1 | Repo holds a C++ code scaffold only. No `.uasset`/`.umap` files, no map, no Data Asset instance, no stadium model. | `find` over the repo (164 KB total) |
| F2 | The scaffold originally targeted `EngineAssociation` `"5.4"`. It was retargeted to `"5.8"` in cycle 2 at the user's direction. No installed engine has been verified. | `VirtualStadium.uproject` |
| F3 | The `CesiumForUnreal` plugin is enabled in the `.uproject`, but no Cesium version is pinned and `VirtualStadium.Build.cs` has no Cesium module dependency. No C++ uses Cesium. | `.uproject`, `Build.cs` |
| F4 | Existing classes: `FStadiumScheduleStop` (struct), `UStadiumScheduleDataAsset`, `AStadiumTourCamera` (a CameraActor that owns step state, input, the widget and marker spawning), `AStadiumStopMarker`, `UStadiumTourWidget`. | `Source/VirtualStadium/**` |
| F5 | Steps store raw Unreal world positions (`WorldLocation`, `CameraRotation`). They aren't tied to a named target or a georeferenced frame. | `StadiumScheduleStop.h` |
| F6 | Next and Previous wrap around (`% StopCount`). Pressing Next on the last step jumps back to step 1 with no warning. | `StadiumTourCamera.cpp` |
| F7 | The camera moves on Tick with an ease-in-out lerp and slerp. There is no Pause, reduced-motion mode, settings, audio, disclaimer level or inactivity reset. | `StadiumTourCamera.cpp` |
| F8 | Input uses legacy `ActionMappings` (`TourPrevious`/`TourNext`/`TourRestart`). Enhanced Input isn't used. | `Config/DefaultInput.ini` |
| F9 | `ScheduleStops.template.json` (8 stops, placeholder coordinates) isn't loaded at runtime. It has to be copied into the Data Asset by hand, so the two can drift apart. | `Docs/ManualSetup.md` |
| F10 | `DefaultEngine.ini` sets no default map. `README.md` repeats its content twice. | Config, README |
| F11 | `README.md` links a **CGTrader** stadium model. The brief names a **3dmodels.org** model. The two may be different assets. | `README.md` and the brief |
| F12 | There is no `.gitattributes` and Git LFS isn't configured, so the repo isn't ready for large binary assets. | Repo root |
| F13 | The session container runs Linux with 4 CPUs, 15 GB RAM and no GPU. It has no Unreal Engine, Blender or .NET, so it **can't compile, run PIE or package**. | Shell inspection |
| F14 | Egress to `cesium.com`, `dev.epicgames.com` and `3dmodels.org` is blocked for both the shell and WebFetch. WebSearch works but only returns snippets. | `curl`, WebFetch |
| F15 | The user directed on 2026-09-28: **use UE 5.8 and the Unreal MCP/CLI.** | User |
| F16 | No Unreal MCP server is configured or reachable in this cloud session. The MCP registry has no Unreal connector. Unreal MCP is reported to run inside the editor on `127.0.0.1`, so it has to be driven from Claude Code on the engine workstation. | ToolSearch, SearchMcpRegistry, web search |
| F17 | Web search reports: UE 5.8 released 2026-06-17 with an **experimental** first-party Unreal MCP plugin; Cesium for Unreal v2.28.0 (July 2026) is the first release supporting UE 5.8. Not checked against official pages. | Web search snippets only |

## 2. Assumptions (temporary, dated 2026-09-28; each one needs confirming)

| # | Assumption | Invalidated if |
|---|---|---|
| A1 | The target is UE 5.8 on Win64 with Cesium for Unreal v2.28.0+. | The workstation shows other versions. |
| A2 | The deliverable for the October 9 demo is a packaged Win64 executable on one large display. Editor PIE is the fallback. | The user says otherwise. |
| A3 | V1 collects no personal data and has telemetry off. | — |
| A4 | Every interior location (gates, security, restrooms, concessions, sensory room, seats) is **Illustrative** until stadium operations verify it. | Verified data is supplied. |
| A5 | Only Eagles, stadium or sponsor branding that someone has approved will ship. Placeholders use neutral art. | Legal or the Eagles Autism Foundation approves branding. |
| A6 | The existing scaffold will be refactored rather than rewritten. The Data Asset, widget base and marker are all reusable. | Code review shows otherwise. |
| A7 | Golden-path steps: exterior arrival → gate entry → interior placeholder → approximate seat view. | — |

## 3. Open questions
See the blocking questions in the Phase 0 reply (Q1–Q7). The answers go here.

## 4. Decisions
| ID | Decision | Status |
|---|---|---|
| D0 | Keep the project docs in `/Docs` (existing convention). The ledger is this file. | Accepted |
| D1 | Default hypothesis: one fixed local origin at the stadium, no continuous origin shifting in the guided mode, Cesium tiles clipped under the stadium, placement stored in one config asset. | Proposed (Cesium agent to confirm) |
| D2 | Default hypothesis: journey state is driven at runtime from a Data Asset. Sequencer is used only for optional cinematics. | Proposed |
| D3 | Target UE 5.8 and Cesium for Unreal v2.28.0+. | Accepted (user) |
| D4 | Two-lane workflow. Engine-bound work (compile, PIE, asset creation, import, alignment, packaging) runs in Claude Code on the engine workstation via Unreal MCP and the UE CLI (`Docs/LOCAL_AGENT_SETUP.md`). The cloud session does code, config and docs on the same branch. Don't tunnel the MCP port. | Accepted |
| D5 | Unreal MCP is experimental, so it's a productivity tool, not a build dependency. Every gate needs CLI-reproducible evidence (build and package logs), not MCP-only evidence. | Accepted |

## 5. Risks (top, pre-architecture)
| # | Risk | Sev | Mitigation |
|---|---|---|---|
| R1 | No engine in the agent environment, so compile, run and package can't be verified by the lead. | High | Treat all C++ as unverified until a human engineer builds it. Ship build and validation scripts plus exact editor steps. |
| R2 | The stadium asset isn't in hand, the vendor is ambiguous (F11) and the license is unverified. | High | Use a labeled proxy stadium plus a gated ingestion SOP. |
| R3 | The 1.8M-poly asset hurts performance and repo size. | High | Measure before committing. Use LFS or keep it outside git. Section the model. |
| R4 | Raw world coordinates in steps break if the georeference origin changes (F5). | High | Steps reference named targets in a fixed local frame. |
| R5 | Placeholder accessibility info could be mistaken for real navigation. | High | A mandatory DisclaimerLevel on every step, with an on-screen banner. |
| R6 | A Cesium ion token or network access may be missing on the demo machine. | Med-High | Decide on an offline fallback (cached or local tiles, or a proxy ground). |
| R7 | Brand and trademark use hasn't been approved. | Med | Neutral placeholders. Legal review is part of Gate 7. |
| R9 | Unreal MCP is experimental. Tool names and behavior may change or break mid-project. | Med | D5: CLI evidence is authoritative. MCP is optional acceleration. |
| R8 | The wrap-around Next (F6) breaks predictability. | Low (easy fix) | Stop at the final step and show an explicit “Start again” action. |

## 6. Dependencies / missing inputs
- BLOCKED: a UE 5.8 workstation running Claude Code with Unreal MCP connected. Also unknown: Launcher build vs source build, and the exact 5.8.x and Cesium versions.
- BLOCKED: the purchased asset archive and its license text.
- BLOCKED: target hardware, display and input specs, and whether the demo machine is online.
- BLOCKED: a Cesium ion account or token policy.
- BLOCKED: stakeholder approvers for placeholder copy, branding and accessibility claims.

## 7. Required stakeholder inputs
Eagles Autism Foundation, stadium operations, security, autistic advisors and accessibility specialists, and legal. See Gate 7.

## 8. Acceptance criteria
The golden path (see the brief) plus quality gates 1–7.

## 9. Implementation status
| Gate | Status |
|---|---|
| G1 Versions/license/asset/hardware/deploy known | OPEN — engine version decided (5.8); install, asset, hardware and deploy mode still unknown |
| G2 Model imports and has a perf baseline | OPEN — asset not available |
| G3 Stadium aligns reproducibly with Cesium | OPEN |
| G4 Four-stop golden path works | OPEN (scaffold partially covers it) |
| G5 Accessibility controls and reduced motion | OPEN |
| G6 Packaged build on target hardware | OPEN — agent can't package (F13) |
| G7 Stakeholder approval | OPEN |
