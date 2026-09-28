# Project Ledger — LFF Virtual Game-Day Schedule

Living document. Update every cycle. Tags: **VERIFIED** (inspected this repo/session), **ASSUMED** (temporary, dated), **BLOCKED** (needs an input), **UNKNOWN**.

Last updated: 2026-09-28 (cycle 5: first workstation compile, tests, MCP, and scripted level and HUD on the Mac)

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
| F18 | User statement: **UE 5.8.3 is installed** on the user's workstation. | User, 2026-09-28 |
| F19 | User statement: use the **latest public Cesium for Unreal from Fab** (listing `76c295fe-0dc6-4fd6-8319-e9833be427cd`). The exact version gets recorded at install. | User |
| F20 | User statement: the stadium model is the **CGTrader** "Lincoln Financial Field Philadelphia USA" listing (this resolves F11). It isn't downloaded yet. Use a placeholder until then. | User |
| F21 | User statement: the demo machine is a **desktop PC or Mac**. Input is keyboard and mouse, with a game controller as backup. It is not a touch kiosk. | User |
| F22 | User statement: the **Oct 9 deliverables** are (a) a packaged executable downloadable from **GitHub Releases** and (b) a **video recording** of the full experience for a TV or screen. | User |
| F23 | User statement: **everything in V1 is Illustrative.** Assume the visitor's seat comes from a ticket scan: use an **example seat** and the journey locations near it. | User |
| F24 | User statement: the user will approve placeholder wording together with the Eagles Autism Foundation. **The user approved use of official Eagles branding.** | User |
| F25 | User statement: **Calm Mode is an opt-in toggle, off by default.** | User |
| F26 | User statement: **Cesium for Unreal v2.29.1** is installed. | User |
| F27 | User statement: **both Windows and Mac builds are wanted. The current workstation is a Mac.** | User |
| F28 | User statement: **the repo is private.** Releases are shared with the collaborators who need them. | User |
| F29 | User statement: **use Google Photorealistic 3D Tiles.** | User |
| F30 | The Mac workstation has **UE 5.8.3 Launcher build, CL 58210709** at `/Users/Shared/Epic Games/UE_5.8`, Xcode 26.4.1, and 14 cores. This verifies F18 and A2. | `Build.version`, `xcodebuild -version` (workstation) |
| F31 | **Cesium for Unreal 2.29.1** is installed to the engine (`Plugins/Marketplace/`, descriptor version 95, built for 5.8.0). This verifies F26. | `CesiumForUnreal.uplugin` |
| F32 | The Launcher build ships **Unreal MCP** (`ModelContextProtocol`), `Terminal`, `ToolsetRegistry` and the `*Toolset` plugins. No source build is needed. The MCP server defaults to `http://127.0.0.1:8000/mcp`, binds to `localhost` and doesn't auto-start. The config generator is `ModelContextProtocol.GenerateClientConfig ClaudeCode`. This supersedes F16 and F17 for the workstation. | Plugin descriptors and source (workstation) |
| F34 | Unreal MCP is **connected on the Mac**. The enabled plugins are `ModelContextProtocol`, `Terminal`, `EditorToolset`, `UMGToolSet` and `AutomationTestToolset`; the editor wrote them to `.uproject`. The server listens on `127.0.0.1:8000` only. `.mcp.json` is committed and has no secrets. The server exposes 3 top-level tools and 22 toolsets. | `lsof`, MCP `initialize` / `list_toolsets` |
| F35 | `L_LFF_Stadium`, `DT_LFF_GoldenPath` and `WBP_LFF_HUD` now exist. They're built by the scripts in `Tools/Editor/` (LOCAL_AGENT_SETUP §5). The level uses **seed** placement values and **seed** camera poses, placed without tiles; neither has been calibrated or framed. | Build logs, MCP `GetWidgets` |
| F36 | In PIE, the game mode loads, the HUD shows the intro and the controls are in the right states. Google tiles return **401** because no project ion token is selected; the user is signed in to ion in the Cesium panel. | PIE capture, Output Log, 2026-09-28 |
| F33 | The first compile needed two build-rule fixes: the target files moved to `Source/`, and `BuildSettingsVersion.V5` became `Latest`. **The ~1,100 lines of C++ then compiled with 0 errors and 0 warnings**, and the 4 `LFF.*` tests pass. This retires R14 for the Mac. | `Saved/Logs/T1.1-build-mac.log`, `T1.2-tests-mac.log` |

## 2. Assumptions (temporary, dated 2026-09-28)

| # | Assumption | Invalidated if |
|---|---|---|
| A1 | The Mac build is made on the current Mac. **The Windows build needs a Windows PC with UE 5.8.3 and VS 2022**, because Unreal can't cross-compile Win64 from macOS. | A Windows machine or CI is unavailable (see R11). |
| A2 | ~~The workstation uses the UE 5.8.3 Launcher build.~~ Verified as F30. | — |
| A3 | V1 collects no personal data and has telemetry off. The "ticket" is a **synthetic example** stored in a text data file. No real ticket data or scanning. | — |
| A4 | Example ticket: **Section 111, Row 10, Seat 8**, Gate and nearby amenities all Illustrative. The user can change it in one JSON file. | The user or the Eagles Autism Foundation picks another seat. |
| A5 | Branding approval (F24) covers the Eagles marks in the demo. The logo files come from the user or the Eagles Autism Foundation. Sponsor or third-party marks not owned by the Eagles are still reviewed case by case. | — |
| A6 | The repo is private (F28). A restricted ion token is still used, because packaged builds can be unpacked. | — |

## 3. Open questions (non-blocking unless marked)
- Q-A: **Which Windows PC will build Win64?** Also, is an Apple Developer ID available to notarize the Mac build? Without one, collaborators get the Gatekeeper workaround.
- Q-C: Resolved: Google Photorealistic 3D Tiles (F29).
- Q-D: Does the CGTrader license allow distributing the model inside a packaged executable on GitHub Releases? Check the license text when you buy it.

## 3b. Open items found while building (2026-09-28)
- **Resolved: the toggles only appeared in one panel.** C++ now binds an optional `TogglesPanel`, shown when `LFFJourneyRules::ShouldShowToggles(bStarted, bSettingsOpen)` is true (`!bStarted || bSettingsOpen`; covered by `LFF.Journey.Rules`). `WBP_LFF_HUD` puts one set of toggles in it, with the intro and Settings headers above. Sound stays in the top bar, so T2 row 11 can pass.
- **HUD wording is placeholder** (headings, pause, restart and settings text) until the user and the Eagles Autism Foundation approve it (F24).

## 4. Decisions
| ID | Decision | Status |
|---|---|---|
| D0 | Project docs live in `/Docs`. This file is the ledger. | Accepted |
| D1 | Geospatial: fixed cartographic origin at midfield turf; `LFF_SiteRoot` is the only GlobeAnchored actor; no origin shifting; polygon clipping once the model arrives. For V1, placement is stored in the map and recorded in CESIUM_PLACEMENT (the Data Asset is deferred by ADR-0003). See ADR-0001. | Accepted, amended |
| D2 | Journey: plain-C++ state machine plus `ULFFJourneySubsystem` (WorldSubsystem); runtime camera transitions; Sequencer only for optional cinematics. See ADR-0002, reduced to 8 classes by ADR-0003. | Accepted, amended |
| D3 | UE 5.8.3 plus Cesium for Unreal v2.29.1. | Accepted (user) |
| D4 | Two-lane workflow: engine work runs in Claude Code on the workstation (Unreal MCP + CLI); code and docs are done in the cloud. | Accepted |
| D5 | Gate evidence must be reproducible from the CLI. MCP is only there to speed things up. | Accepted |
| D6 | **Naming reconciled.** C++ types use `LFF` without an underscore (`ALFFCameraTarget`, `ULFFJourneySubsystem`). Assets use `LFF_` (`DT_LFF_GoldenPath`, `WBP_LFF_HUD`). The site root actor and tag is `LFF_SiteRoot`. The map is `/Game/LFF/Levels/L_LFF_Stadium`. Camera targets are `ALFFCameraTarget`, which replaces the Cesium draft's `AStadiumJourneyTarget`. | Accepted |
| D7 | Step data is a **Data Table imported from `SourceData/Journey/LFF_GoldenPath.json`** (diffable in git), with a drift test. This replaces the Data Asset and the hand-copied template (F9). | Accepted |
| D8 | Steps reference `ALFFCameraTarget` by `FName TargetId`. A missing target fails closed ("This step is not available yet"). | Accepted |
| D9 | V1 steps are all `DisclaimerLevel = Illustrative` (F23). The Verified and Approximate values stay in the enum for later. | Accepted |
| D10 | **Example ticket.** Section 111, Row 10, Seat 8 is written straight into the step JSON. There's no runtime JSON read (packaged builds don't stage SourceData). The badge reads "Prototype · Illustrative · Example ticket", and the copy never says "scan". | Accepted (supersedes the token design) |
| D11 | Visitor settings are never saved to disk. They're **kept across Restart** (so a visitor who needs reduced motion doesn't lose it) and reset only when the app launches. | Accepted (revised) |
| D12 | Inactivity reset is **cut from V1** and moved to Tier B. It's a desktop app for families, and a timeout could lose a slow reader's place. When it's added, use the 120/30/300 s values. | Accepted (revised) |
| D13 | Defaults: sound **off**, reduced motion **off** (one-click toggle on the intro screen), Calm Mode **off**, opt-in (F25). The noise preview is **deferred to Tier B**. The API is stubbed but disabled. | Accepted |
| D14 | **Video deliverable.** No RecordMode in the app. An operator follows the TEST_PLAN T4 shot list and captures the packaged Development build with OBS. First take by Oct 5. Movie Render Queue is Tier B. | Accepted (revised) |
| D15 | **Release deliverable.** Mac and Win64 Development zips go on the private repo's GitHub Releases, each under 2 GiB. The builds use a restricted Cesium ion token scoped to Google Photorealistic 3D Tiles, rotated after the demo. They're unsigned unless an Apple Developer ID is available, so the release notes cover the Gatekeeper and SmartScreen steps. | Accepted |
| D16 | Input: keyboard and mouse first, controller second. Touch isn't targeted in V1, but the UI keeps large hit areas. | Accepted |
| D18 | V1 code is the 8 classes plus tests in ADR-0003. Input is built in code. The HUD is one C++ base plus one WBP. | Accepted |
| D19 | Restart always asks "Start again?". "Start again" at the end of the journey is direct. There are no hold or timed interactions. | Accepted |
| D20 | Keyboard shortcuts are the main keyboard path. Buttons hand focus back to the game after each click. Tab navigation is a known limitation. | Accepted |
| D21 | **Google Photorealistic 3D Tiles** are the context. Their stadium mesh is the exterior stand-in until the CGTrader model arrives, so clipping stays off until then. Attribution stays visible. The build is online-only. | Accepted |
| D22 | Each platform is packaged on its own OS: Mac on the current Mac, Win64 on a Windows PC. | Accepted |
| D23 | `Content/CesiumSettings/CesiumIonServers/` (which holds `DefaultIonAccessToken`) is **git-ignored and untracked**. The token is chosen per machine. This supersedes committing `CesiumIonSaaS.uasset` in `c567429`, which never held a token. | Accepted (lead, per CESIUM_PLACEMENT §7) |
| D24 | Level and HUD setup is **scripted** (`Tools/Editor/`) rather than hand-built, so it can be reproduced on the Windows PC and after a model swap. Hand framing of the `CT_` targets happens after the scripts run, and the scripts never overwrite. | Accepted |
| D17 | Gate waivers to start implementation: **G2 waived for V1** (proxy stadium; the real model imports through ASSET_INGESTION when it arrives). **G3 partly waived.** The journey code doesn't depend on alignment. Calibration happens on the workstation before the Oct 9 build. | Accepted, risk R2/R4 owned by lead |

## 5. Risks (top, pre-architecture)
| # | Risk | Sev | Mitigation |
|---|---|---|---|
| R1 | No engine in the agent environment, so compile, run and package can't be verified by the lead. | High | Treat all C++ as unverified until a human engineer builds it. Ship build and validation scripts plus exact editor steps. |
| R2 | The stadium asset isn't in hand and the CGTrader license terms for public redistribution are unverified (Q-D). | High | Proxy stadium (D17), gated ingestion SOP; confirm the license before shipping a Release that contains it. |
| R3 | The 1.8M-poly asset hurts performance and repo size. | High | Measure before committing. Use LFS or keep it outside git. Section the model. |
| R4 | Raw world coordinates in steps break if the georeference origin changes (F5). | High | Steps reference named targets in a fixed local frame. |
| R5 | Placeholder accessibility info could be mistaken for real navigation. | High | A mandatory DisclaimerLevel on every step, with an on-screen banner. |
| R6 | A Cesium ion token or network access may be missing on the demo machine. | Med-High | Decide on an offline fallback (cached or local tiles, or a proxy ground). |
| R7 | Brand use: the Eagles marks are approved by the user (F24). Third-party and sponsor marks are not covered. | Low-Med | Review textures and copy for non-Eagles marks. |
| R10 | A public Release exposes the Cesium ion token and the model data. | Med-High | D15 restricted token; Q-B; Q-D. |
| R11 | **No Windows build machine identified yet.** Unreal can't cross-compile Win64 from macOS. | High | Q-A: name a Windows PC with UE 5.8.3 + VS 2022 by Oct 3, or ship the Mac build only with Windows to follow. |
| R13 | Google tiles are online-only, and their use is metered through Cesium ion. | Med | Restricted token; confirm the ion plan quota; the video is the offline fallback. |
| R14 | The C++ was written without a compiler (about 1,100 lines). | Low (Mac) | **Retired on the Mac (F33).** Win64 (MSVC) is still uncompiled, so T1.3 stays open. |
| R12 | Only 11 days remain until Oct 9, and all engine work depends on one workstation. | High | Keep scope to the golden path. Record the video early as a fallback. |
| R9 | Unreal MCP is experimental. Tool names and behavior may change or break mid-project. | Med | D5: CLI evidence is authoritative. MCP is optional acceleration. |
| R8 | The wrap-around Next (F6) breaks predictability. | Low (easy fix) | Stop at the final step and show an explicit “Start again” action. |

## 6. Dependencies / missing inputs
- READY: a UE 5.8.3 workstation (F30), Cesium 2.29.1 (F31), Claude Code running there, and Unreal MCP enabled and answering on `127.0.0.1:8000` (F34).
- PENDING: the CGTrader model and its license (F20). A proxy is used until then.
- KNOWN: desktop PC or Mac, keyboard and mouse plus a controller (F21). UNKNOWN: GPU, and whether the machine is online.
- BLOCKED: a Cesium ion account or token policy.
- KNOWN: the user plus the Eagles Autism Foundation approve the wording, and Eagles branding is approved (F24).

## 7. Required stakeholder inputs
Eagles Autism Foundation, stadium operations, security, autistic advisors and accessibility specialists, and legal. See Gate 7.

## 8. Acceptance criteria
The golden path (see the brief) plus quality gates 1–7.

## 9. Implementation status
| Gate | Status |
|---|---|
| G1 Versions/license/asset/hardware/deploy known | MOSTLY MET: UE 5.8.3 (CL 58210709), Cesium 2.29.1, CGTrader model, desktop target, Releases + video. Open: model license (Q-D), GPU. |
| G2 Model imports and has a perf baseline | WAIVED for implementation start (D17). Proxy stadium. |
| G3 Stadium aligns reproducibly with Cesium | PARTLY WAIVED (D17). Strategy accepted (ADR-0001); calibration happens on the workstation. |
| G4 Four-stop golden path works | **T1.1 PASS, T1.2 PASS on the Mac** (2026-09-28, UE 5.8.3 CL 58210709, commit c567429). The map, table and HUD are built (F35), and PIE reaches the intro (F36). T1.3 (Windows) is open. Next: select the ion token, calibrate the origin, frame the 4 targets, then run the T2 PIE checklist. |

### Evidence log
| Date | Machine | Check | Result | Log |
|---|---|---|---|---|
| 2026-09-28 | Mac, UE 5.8.3 | T1.1 editor compile | First run failed (RulesError): the target files were in the module folder. Second run failed: V5 build settings aren't allowed with the shared editor environment. **Third run passed**: exit 0, 14 actions, 0 warnings, 24 s. | `Saved/Logs/T1.1-build-mac.log` |
| 2026-09-28 | Mac, UE 5.8.3 | T1.2 `Automation RunTests LFF.` | **PASS**: found 4 tests (StateMachine.EdgeCases, .Navigation, .PauseRestart, Journey.Rules), 4 Success, exit 0. Unrelated: Cesium logs an uninitialized-enum error for `FCesiumMetadataPropertyStatisticValue::Semantic` in its own headers. | `Saved/Logs/T1.2-tests-mac.log` |
| 2026-09-28 | Mac, UE 5.8.3 | `build_lff_stadium_level.py` (headless) | **PASS**: 4 table rows imported; `validate OK: 1 georeference, 1 anchor (LFF_SiteRoot), 0 origin shift, 4 targets`; saved. Exit 1 comes from the known editor-shutdown crash after the save (LOCAL_AGENT_SETUP §5). | `Saved/Logs/build-level-full.log` |
| 2026-09-28 | Mac, UE 5.8.3 | `build_wbp_lff_hud.py` (MCP) | **PASS**: 94 widgets, all 30 contract names, compiled, saved. | Script output |
| 2026-09-28 | Mac, UE 5.8.3 | PIE smoke test (not T2) | **PASS (intro only)**. `Game class is 'LFFGameMode'`. The HUD intro shows the card copy, badge, disclaimer and the two toggles; Back, Start again and Pause are disabled; Start is enabled. No LFF errors. Tiles: 401 (no token). Input wasn't driven, because MCP has no input tool. | Editor capture |
| 2026-09-28 | Mac, UE 5.8.3 | TogglesPanel change: T1.1 and T1.2 re-run, HUD rebuilt | **PASS**. The build succeeded with 0 warnings (9 actions). All 4 `LFF.*` tests pass, and `Rules` now includes 4 `ShouldShowToggles` cases. The HUD rebuilt with 93 widgets and all 31 contract names. In PIE, the intro shows TogglesPanel with the intro header and 3 toggles, and the Settings header is hidden. No LFF or UMG errors. | `Saved/Logs/T1.*-mac.log` |
| 2026-09-28 | Mac, UE 5.8.3 | Unreal MCP smoke test (LOCAL_AGENT_SETUP §2 step 6) | **PASS**. `get_current_level` returned `/Temp/Untitled_0`, and `find_actors` returned 7 default actors (WorldSettings, Brush_0, DefaultPhysicsVolume_0, BuoyancyManager_0, GameplayDebuggerPlayerManager_0, ChaosDebugDrawActor, CesiumGaussianSplatSystemActor). Run over raw JSON-RPC, because this Claude Code session started before `.mcp.json` existed. | This row |
| G5 Accessibility controls and reduced motion | OPEN |
| G6 Packaged build on target hardware | OPEN. Mac on this workstation; Win64 blocked on R11. |
| G7 Stakeholder approval | OPEN. Approvers: the user plus the Eagles Autism Foundation. |
