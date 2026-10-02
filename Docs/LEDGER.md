# Project Ledger — LFF Virtual Game-Day Schedule

Living document. Update every cycle. Tags: **VERIFIED** (inspected this repo/session), **ASSUMED** (temporary, dated), **BLOCKED** (needs an input), **UNKNOWN**.

Last updated: 2026-09-30 (session 3: splash orbit, Oct 18 1:00 PM timeline, voiceover, scene cues, hero stand-ins)

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
| F37 | **Google tiles stream and render** once a project token is chosen and `ExtendDefaultLuminanceRange=True` is set. The hand-written `DefaultEngine.ini` lacked that UE5 default, so CesiumSunSky blew every frame out to white even though the tiles had loaded (100% progress, about 52 MB cached). | CVar check, captures |
| F38 | **Placement revision 2026-09-28a** is applied and saved: origin 39.9008036, -75.1674790, -31.04 m ellipsoidal; SiteRoot heading -6.5°. The build script matches it. The gate and seat targets were re-posed from tile traces (gate 35 m outside the north headhouse; seat about row 10 of the west lower bowl). The proxy seat rows were removed because the tiles provide the bowl. | CESIUM_PLACEMENT V1 table |
| F39 | A headless editor run's orphaned `CrashReportClient` can hold MCP port 8000 (it inherits the socket). Fixed by giving headless runs `-ModelContextProtocolPort=18000`. | `lsof`, editor log |
| F40 | **The official CGTrader model is integrated (ADR-0004).**<br>• 26 Nanite chunks, 6.9 M triangles, 95 PBR material instances, 140 textures.<br>• Registered to the tiles at 182.25° and (−2.25, +2.5) m, median residual 0.61 m.<br>• The Google tiles are clipped over the model site. The tiles still load in the editor background (throttling off).<br>• The proxy concourse is replaced by the model's plaza-level concourse. | Pipeline logs, captures |
| F41 | **Lighting is now UE5-grade.** Lumen GI and reflections, VSM, distance fields, TSR, `CachedLightingPreExposure=8`, and `PPV_LFF_Cinematic` (EV 7–15; no motion blur, flares, grain or chromatic aberration). The previous config had no GI at all. | `DefaultEngine.ini`, `setup_cinematic_look.py` |
| F42 | **Performance on the Mac at 1080p, intro view:** frame 16.9 ms (60 fps vsync), GPU 12.4 ms, Nanite 0.35 ms. | `Docs/PERF_BASELINE.md` |
| F43 | **The sky is 1:00 pm EDT on Oct 18, 2026, partly cloudy.** The sun is at 40.3° elevation and 184.6° azimuth, matching the hand calculation. The clouds are static (0% pixel change over 10 s). At 1080p on the M4 Max it holds 60 fps mean, GPU 14.3 ms (PERF_BASELINE). | Captures, CSV |
| F44 | **There were no sound assets** in `/Game` before session 2 (MCP `find_assets` for SoundWave, SoundCue, SoundClass, SoundMix), so Sound On had nothing to play. Two synthesized placeholders now exist in `/Game/LFF/Audio`. | MCP asset search, import log |
| F45 | **The stadium model has collision** on its exterior (a gate→concourse trace hits at 43 m), but **the plaza-level concourse walls and deck don't** (horizontal and vertical traces pass through). | `SceneTools.trace_world` |
| F46 | In PIE the audio device reports "Using Audio Hardware Device Unknown". Audio playback is evidenced by log lines only; nobody listened during session 2. | Output Log |
| F47 | **None of the three Fab packs are on the Mac** (City Sample Vehicles, MetaHuman Crowd Sample, Free Crowd Cheering Sounds): nothing under `/Users/Shared/Epic Games`, the Fab plugin cache or Spotlight. Adding them needs the Epic account's Fab library (licence acceptance), so they were not imported. | Disk search, 2026-09-30 |
| F48 | `Assets/bald-eagle` is a Cinema 4D glTF: 3 skins (12/15/15 joints), 296 meshes (293 rigid feathers, some animated on their own nodes), one 2 s animation, 15.8 k triangles. A skeletal import would lose parts of the flap, so it's baked to Alembic in Blender 5.1.1 and imported as a GeometryCache (48 frames, 8 material slots). | glTF inspection, `[LFF content]` log |
| F49 | `Assets/EaglesMobileTicket.gif` is 730×1062, 60 frames at 20 fps (a slow barcode sweep on a phone). It reads "Eagles vs. Lions, Sec 107, Row 18, Seat 22, South Gate", not the example ticket (A4). | PIL |
| F50 | No logo asset exists in the project, so the finale shows no logo. | Content search |
| F51 | In the editor, `TextRenderComponent.TextRenderColor` takes 0–1 channels over MCP; `add_to_scene_from_class` with a parent takes a relative transform. | MCP `get_properties`, actor transforms |
| F52 | A 5.8.3 editor in the background runs MCP calls at about 3 per second (the stand-in build takes minutes instead of 35 s). | Editor log timestamps |
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
- **Resolved: labels were hard to read against the tiles.** Every label is now a sign: white text on a dark midnight-green plate (`MI_LFF_SignPlate`). The plate is a component of the label actor, so it shows and hides with it. Text and plate are both lit, so their contrast (about 10:1 by albedo) holds in sun and shade. The gate and concourse signs are larger. The section sign is smaller and sits 3 m ahead in the lower third.
- **Resolved by ADR-0004 (the model replaces the proxy; the tiles are clipped over the site). Was open: tile geometry intrudes into the concourse proxy.** Traces find tile surfaces inside the corridor, including one at 6 m height about 2.5 m in front of `CT_InteriorConcourse`. The corridor sits inside the west-stand photogrammetry shell under a flat deck at 8.2 m. **Proposed fix:**
  - Move the corridor underground, where the tiles have no geometry.
  - Add a C++ rule so a move is a fade if **either** end is authored `ReducedMotion`.
  - Author `interior-concourse` as `ReducedMotion`.

  Moves into and out of the interior would then cut rather than fly through the ground or facade. They already fly through the facade today. This needs the user's OK, because it changes how the journey feels.
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
| D18 | V1 code is the 8 classes plus tests in ADR-0003. Input is built in code. The HUD is one C++ base plus one WBP. | Superseded for the HUD by D26 |
| D19 | Restart always asks "Start again?". "Start again" at the end of the journey is direct. There are no hold or timed interactions. | Accepted |
| D20 | Keyboard shortcuts are the main keyboard path. Buttons hand focus back to the game after each click. Tab navigation is a known limitation. | Accepted |
| D21 | **Google Photorealistic 3D Tiles** are the context. Their stadium mesh is the exterior stand-in until the CGTrader model arrives, so clipping stays off until then. Attribution stays visible. The build is online-only. | Accepted |
| D22 | Each platform is packaged on its own OS: Mac on the current Mac, Win64 on a Windows PC. | Accepted |
| D23 | `Content/CesiumSettings/CesiumIonServers/` (which holds `DefaultIonAccessToken`) is **git-ignored and untracked**. The token is chosen per machine. This supersedes committing `CesiumIonSaaS.uasset` in `c567429`, which never held a token. | Accepted (lead, per CESIUM_PLACEMENT §7) |
| D24 | Level and HUD setup is **scripted** (`Tools/Editor/`) rather than hand-built, so it can be reproduced on the Windows PC and after a model swap. Hand framing of the `CT_` targets happens after the scripts run, and the scripts never overwrite. | Accepted |
| D25 | **The official model replaces the Google stadium (ADR-0004).** The tiles are clipped over the model site and the NFL field is used. It's prepared in Blender, imported as Nanite, and the site is registered to the tiles by height matching. | Accepted (user direction 2026-09-29) |
| D26 | **HUD restyle (2026-09-29, user spec).** One HUD (`WBP_LFF_HUD`) holds five panels: Splash, TopChrome, StepCaption, BottomRail and Settings, with a shared C++ parent `ULFFJourneyPanel`. The splash has no Back, Start again or toggles. Steps show a thin top bar, one lower-left caption card and a Back · pips · Next rail. Start again, the toggles and the keyboard legend live in Settings, which opens over a dimmed world. Back on step 1 returns to the splash. A **High contrast** toggle was added (presentation only). The step disclaimer is now a one-word chip (Verified / Approximate / Illustration); the full "not a navigation instruction" sentence shows on the splash only. **Needs owner sign-off** against D9. Captions were **not** added: nothing in V1 speaks, so there is nothing to caption. Plain UMG, not Common UI: Common UI needs its own viewport client and input setup, which the spec ruled out. | Accepted (disclaimer change pending) |
| D27 | **Session 2 (user spec, 2026-09-29).** HUD shrunk in place (no new HUD): caption card about 450 px at 1080p, 64 px hit targets. The one-line disclaimer under the badge is back on steps; the chip stays. Sound Off mutes the device; Sound On is one low ambient bed. Calm Mode = Less movement, Sound off, crowd hidden, previews locked off, cuts only. The journey is the 16 EAF visual-schedule beats, all Illustrative, with cheap stand-in props. Steps 11–12 offer "Preview this? Off / Soft", never autoplayed, never flickering. No free roam, no crowds, no spline paths, no Cesium or import changes. Supersedes D13's "noise preview deferred" and D26's type sizes. | Accepted (user) |
| D28 | **Session 3 (user spec, 2026-09-30).** The example game is Sunday, Oct 18, 2026, Panthers at Eagles, kickoff 1:00 PM. The brief's 23 steps replace session 2's 16; timed steps are `Approximate` with example clocks, the rest `Illustrative`. The card is the voiceover script and the caption; Voice is Off by default, needs Sound, is forced off by Calm Mode, stops on every step change and never overlaps a preview. Unreal never calls ElevenLabs: `VO_MANIFEST.md` and the `.txt` lines are the recording brief, and `import_lff_vo.py` brings the files in. The splash orbits (Calm or Less movement: still), Start glides into a wide shot, parking has an arriving car. A clickable timeline sits above Back/Next. Gates, the car, the eagle and fireworks are `ALFFCueActor`s. Crowd is Low or Off, no jerseys, no bowl crowd. The HUD is extended, not rebuilt. Supersedes D27's step list and "no crowds". | Accepted (user) |
| D29 | Placeholder voice lines are generated with macOS `say` so playback, ducking and stop-on-step can be tested before recording. They're marked as placeholders in the manifest and must be replaced before any release. | Accepted (lead) |
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
| R15 | Placeholder `say` voices or stand-in props ship by mistake. | Med | Voice is Off by default; the manifest says PLACEHOLDER; KNOWN_LIMITATIONS lists every stand-in; replace before the Oct 9 build. |
| R16 | The example-game clocks are read as confirmed Oct 18 operations. | Med | Every timed step is `Approximate` (chip and line), the timeline says "Approximate times · Example 1:00 PM game", and the finale says it's an illustration. Update when stadium ops publishes the week's guide. |
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
| G2 Model imports and has a perf baseline | **MET on the Mac** (F40, F42). Open: packaged size under 2 GiB and the Win64 numbers. |
| G3 Stadium aligns reproducibly with Cesium | PARTLY WAIVED (D17). The site frame is calibrated to the tiles (F38: ±1 m, ±1°, ±0.3 m). The model alignment checks V3–V8 wait for the CGTrader model. |
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
| 2026-09-28 | Mac, UE 5.8.3 | Tiles plus calibration pass | **PASS (provisional)**. The tileset loads to 100%. Overhead captures at 3 km, 800 m, 250 m and 120 m show midfield about 1.8 m east and 0.4 m north of the seed; the traced turf is -1.04 m below the seed origin; pitch-wall traces give a heading of about -6°. After the change, turf reads Z ≈ -0.2 m and the walls are symmetric within about 1 m. The target captures show the north headhouse (steps 1–2) and a row-10 view across the pitch (step 4). | Captures in session scratchpad; values in CESIUM_PLACEMENT |
| 2026-09-28 | Mac, UE 5.8.3 | Unreal MCP smoke test (LOCAL_AGENT_SETUP §2 step 6) | **PASS**. `get_current_level` returned `/Temp/Untitled_0`, and `find_actors` returned 7 default actors (WorldSettings, Brush_0, DefaultPhysicsVolume_0, BuoyancyManager_0, GameplayDebuggerPlayerManager_0, ChaosDebugDrawActor, CesiumGaussianSplatSystemActor). Run over raw JSON-RPC, because this Claude Code session started before `.mcp.json` existed. | This row |
| 2026-09-29 | Mac, UE 5.8.3 | D26 HUD restyle: T1.1, T1.2, `build_wbp_lff_ui.py`, PIE | **PASS**. The build succeeded with 0 warnings (10 actions). 4/4 `LFF.*` pass (Rules now covers `ShouldBackReturnToIntro`). All 6 widgets built with their contract names, compiled and saved. PIE, driven by real mouse and key events: splash → Start Your Day → step 1 → Settings open (High contrast on/off) → Close → Back to the splash → Start → Next (step 2) → Back (step 1); the keyboard repeated ←, S, S, ←; steps 3–4 reached, and the last step reads "Start again". Open: the lower-left caption overlaps the world labels on steps 2 and 4 in the 1.56:1 PIE viewport. | `LogVirtualStadium` Verbose snapshot lines, editor captures |
| 2026-09-29 | Mac, UE 5.8.3 | Session 2: T1.1, T1.2 | **PASS**. Clean compiles, 0 warnings (last run: 10 actions). 4/4 `LFF.*` pass; `Rules` now also covers `ResolveMove`, `IsSoundEffective`, the preview rules and `ShouldHideLabelAt`. | `Saved/Logs/T1.*-mac.log` |
| 2026-09-29 | Mac, UE 5.8.3 | Session 2: `import_lff_content.py`, `build_wbp_lff_ui.py --rebuild`, `build_lff_standins.py` | **PASS**. 16 rows imported; both sounds imported (24.0 s looping, 8.0 s). 6 widgets rebuilt with all contract names. 11 targets, 12 props and 11 labels placed and the level saved. | Script output |
| 2026-09-29 | Mac, UE 5.8.3 | Session 2 PIE (real key and mouse events) | **PASS**. Splash and step 1 captured before and after the shrink. All 16 steps reached, with steps 1, 2–9, 10–16 captured. Log evidence: "Sound off: primary volume 0" / "Sound on … Ambient bed playing: SW_LFF_AmbientPlaza (looping=1)". Step 11 plays "SW_LFF_PreviewCrowdMuffled (8.0 s)" only after V, with the button disabled while Sound is off, and "finished" 8.7 s later. Step 12 "Lights preview on (4.0 s cycle, up to +0.9 stops)": sampled luminance 154→158→164→167. Calm Mode on: Sound off, less movement, preview controls hidden, pulse off, then "Instant to CT_GuestServices" / "Instant to CT_SensoryRoom" (finished the same ms). Wall check: "the straight path to CT_Staff crosses geometry; fading and cutting". Fixed during the run: Sound off now also resets a Sound preview, so it can't restart by itself. | Editor captures, `LogVirtualStadium` |
| 2026-09-30 | Mac, UE 5.8.3 | Session 3: T1.1, T1.2 | **PASS**. The build succeeded after one fix (the protected `UButton::InitIsFocusable`), 0 warnings. 5/5 `LFF.*` pass (new `StateMachine.Jump`; `Rules` covers voice, crowd, cue states, the orbit and the anthem preview). | `Saved/Logs/T1.*-mac.log` |
| 2026-09-30 | Mac, UE 5.8.3 | Session 3: `import_lff_content.py` (headless) | **PASS**. 23 rows; 3 sounds (both beds loop); 23 of 23 voice lines, none missing; 30 ticket frames; `MI_LFF_FireworkGlow`; `GC_LFF_BaldEagle` 48 frames with 8 materials. | `[LFF content]` log |
| 2026-09-30 | Mac, UE 5.8.3 | Session 3: `build_lff_standins.py`, `build_wbp_lff_ui.py --extend` (MCP) | **PASS**. 10 parked cars + 1 arriving, 15 crowd stand-ins, 9 cue actors (6 gate leaves, car, eagle flight, fireworks), all targets; StepCaption 19 / BottomRail 20 / Settings 62 widgets with every contract name, compiled and saved. | Script output, target captures |
| 2026-09-30 | Mac, UE 5.8.3 | Session 3 PIE (real key and mouse events) | **PASS except one visual re-check.** Splash orbit ("orbit of 400 m at 135 m high, one lap in 40 s"); Start glides along it ("132 deg, 4.8 s"); parking fade-cut with "Scene cue CarArrival playing" (car rolls in, captured mid-roll and parked). Line → security → plaza gates ("Scene cue GatePlaza playing"), timeline mark emphasized only on its step. Sound On + Voice On: "voice playing VO_03_Parking (9.2 s)…; bed ducked -8 dB", "Voice finished."; Next mid-line: "Voice stopped." in the same frame as the step change, the next line only after arrival. Ticket phone rises; popcorn cart and SAV lettering captured. Timeline clicks: "jump to step 17 / 5 / 18". Anthem Soft: "Anthem preview playing: 2 actors, 9.7 s, one pass" then off; eagle captured crossing above the caption. Crowd Off/Low (15 people), Calm Mode: Instant cuts, gate already open, phone already up. Finale shows Learn More + Start Over; Start Over fades to the splash and orbits. **Open:** the first fireworks pass drew black sparks (log: MI_LFF_FireworkGlow "missing usage flag InstancedStaticMeshes"); fixed with the new unlit `M_LFF_Glow` (flag verified on the asset) but not yet re-watched in PIE, because the screen locked. Learn More wasn't clicked (it opens the browser). | Editor captures, `LogVirtualStadium` |
| 2026-09-30 | Mac, UE 5.8.3 | Session 4 finish pass: `finish_pass.py` (MCP), T1.1, T1.2 | **PASS (editor captures; PIE not run by the agent).** 90 editor icons hidden; `CT_Parking` re-aimed (all 11 hero cars in the 70 deg frame); 9 plaza people hidden in game; `SI_LotCars` 882 cars in 65 ISMs (922 free bays, 882 grounded); `LFF_StadiumInterior` box: all 8 seat and concourse targets inside, all 13 outside targets outside; `SI_BowlCrowd` 198 cards; 30 ticket frames re-imported. Editor build 0 warnings; game target compiles; 5/5 `LFF.*` pass. UBT built with the editor open, so the editor hot-reloaded twice (`WBP_LFF_HUD` binding ensure in memory only; no WBP saved since 14:38): **restart the editor before PIE.** | `Saved/Logs/T1.1-build-mac-finish.log`, `T1.2-tests-mac-finish.log`, editor captures |
| 2026-10-01 | Mac, UE 5.8.3 | Session 5: `extract_seats.py`, `plan_lot.py`, `session5_assets.py` (headless), T1.1, T1.2, `LFF.Shots` offscreen | **PASS (offscreen game captures; PIE not run by the agent).** 56,707 seats; 39,695 bowl + 64 queue MetaHumans spawned, no Mass errors; 811 lot cars on 1,437 stall lines; 10 hero cars + arrival; camera inside → `12 exterior actors (parking lot) hidden`; voice auto-plays and the karaoke word lit at 7.7 s of `VO_03_Parking`; builds 0 warnings; 5/5 `LFF.*`. Open: close-range MetaHumans in A-pose. | `Saved/Screenshots/MacEditor/LFF_step_*.png`, `~/Library/Logs/VirtualStadium/VirtualStadium.log` |
| G5 Accessibility controls and reduced motion | IN PROGRESS: Sound, Voice, Calm Mode, Less movement, Crowd and the previews work in PIE (sessions 2–3). Needs stakeholder review, the recorded voice, and a listening check on the demo machine. |
| G6 Packaged build on target hardware | OPEN. Mac on this workstation; Win64 blocked on R11. |
| G7 Stakeholder approval | OPEN. Approvers: the user plus the Eagles Autism Foundation. |
