# Project Ledger — LFF Virtual Game-Day Schedule

Living document. Update every cycle. Tags: **VERIFIED** (inspected this repo/session), **ASSUMED** (temporary, dated), **BLOCKED** (needs an input), **UNKNOWN**.

Last updated: 2026-09-28 (Phase 1 synthesis, cycle 3)

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

## 2. Assumptions (temporary, dated 2026-09-28)

| # | Assumption | Invalidated if |
|---|---|---|
| A1 | The primary packaged target is **Win64**. A **macOS** build is secondary and happens only if a Mac with Xcode is available to build, sign and notarize it. | The user says the Mac is primary. |
| A2 | The workstation uses the UE 5.8.3 Launcher build (not a source build). | The user says it's a source build. |
| A3 | V1 collects no personal data and has telemetry off. The "ticket" is a **synthetic example** stored in a text data file. No real ticket data or scanning. | — |
| A4 | Example ticket: **Section 111, Row 10, Seat 8**, Gate and nearby amenities all Illustrative. The user can change it in one JSON file. | The user or the Eagles Autism Foundation picks another seat. |
| A5 | Branding approval (F24) covers the Eagles marks in the demo. The logo files come from the user or the Eagles Autism Foundation. Sponsor or third-party marks not owned by the Eagles are still reviewed case by case. | — |
| A6 | The GitHub repo visibility and the audience for Releases are unknown. A public Release makes the build and its Cesium ion token public. | The user states the repo is private. |

## 3. Open questions (non-blocking unless marked)
- Q-A: Is a Win64 build enough for Oct 9, or must there also be a macOS build? (Mac packaging needs a Mac, Xcode, and ideally Apple notarization.)
- Q-B: Is the repo and its Releases public or private? This decides the Cesium ion token scope and the model-license exposure.
- Q-C: Context tiles: Google Photorealistic 3D Tiles (online-only, attribution required) or Cesium World Terrain plus OSM Buildings? The default is Cesium World Terrain + Bing imagery + OSM Buildings via ion, pending your choice.
- Q-D: Does the CGTrader license allow distributing the model inside a packaged executable on GitHub Releases? Check the license text when you buy it.

## 4. Decisions
| ID | Decision | Status |
|---|---|---|
| D0 | Project docs live in `/Docs`. This file is the ledger. | Accepted |
| D1 | Geospatial: fixed cartographic origin at midfield turf; `LFF_SiteRoot` is the only GlobeAnchored actor; no origin shifting; polygon clipping; one `DA_LFF_GeoPlacement`. See ADR-0001. | Accepted (lead), pending workstation verification of the UNKNOWN APIs |
| D2 | Journey: plain-C++ state machine plus `ULFFJourneySubsystem` (WorldSubsystem); `ULFFSettingsSubsystem` (GameInstanceSubsystem); runtime camera transitions; Sequencer only for optional cinematics. See ADR-0002. | Accepted (lead) |
| D3 | UE 5.8.3 plus the latest Cesium for Unreal from Fab. | Accepted (user) |
| D4 | Two-lane workflow: engine work runs in Claude Code on the workstation (Unreal MCP + CLI); code and docs are done in the cloud. | Accepted |
| D5 | Gate evidence must be reproducible from the CLI. MCP is only there to speed things up. | Accepted |
| D6 | **Naming reconciled.** C++ types use `LFF` without an underscore (`ALFFCameraTarget`, `ULFFJourneySubsystem`). Assets use `LFF_` (`DT_LFF_GoldenPath`, `WBP_LFF_HUD`). The site root actor and tag is `LFF_SiteRoot`. The map is `/Game/LFF/Levels/L_LFF_Stadium`. Camera targets are `ALFFCameraTarget`, which replaces the Cesium draft's `AStadiumJourneyTarget`. | Accepted |
| D7 | Step data is a **Data Table imported from `SourceData/Journey/LFF_GoldenPath.json`** (diffable in git), with a drift test. This replaces the Data Asset and the hand-copied template (F9). | Accepted |
| D8 | Steps reference `ALFFCameraTarget` by `FName TargetId`. A missing target fails closed ("This step is not available yet"). | Accepted |
| D9 | V1 steps are all `DisclaimerLevel = Illustrative` (F23). The Verified and Approximate values stay in the enum for later. | Accepted |
| D10 | **Example ticket context.** Add `FLFFTicketContext` (Section, Row, Seat, Gate, all FText) loaded from `SourceData/Journey/LFF_ExampleTicket.json`. Step text can use `{Section}`, `{Row}`, `{Seat}` and `{Gate}` tokens. On screen it's labelled **"Example ticket"**. It's never real data. | Accepted |
| D11 | Visitor settings are never saved to disk. Every restart and inactivity reset restores the ini defaults. | Accepted |
| D12 | Inactivity: warn after 120 s (300 s while paused), then a 30 s countdown, then a calm reset. Values are in ini. This reconciles the A11y draft (300/30) and the Systems draft (60/15) for a staffed desktop demo. | Accepted |
| D13 | Defaults: sound **off**, reduced motion **off** (one-click toggle on the intro screen), Calm Mode **off**, opt-in (F25). The noise preview is **deferred to Tier B**. The API is stubbed but disabled. | Accepted |
| D14 | **Video deliverable.** A non-Shipping `LFF.RecordMode` command walks the golden path with fixed dwell times and captions on screen. It's captured from the **packaged build** with OBS at the TV's resolution. Movie Render Queue is Tier B. The auto-advance exists only in record mode and never in visitor mode. | Accepted |
| D15 | **Release deliverable.** A Win64 Development-or-Shipping zip goes on GitHub Releases, which allows up to 2 GiB per asset. The build uses a **restricted, asset-scoped Cesium ion token** that's created for the demo and rotated afterwards. The build isn't code-signed, so expect a SmartScreen warning (plus a Gatekeeper one on macOS). Document the "Run anyway" steps in the release notes. | Accepted, with Q-B open |
| D16 | Input: keyboard and mouse first, controller second. Touch isn't targeted in V1, but the UI keeps large hit areas. | Accepted |
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
| R11 | The macOS build needs a Mac and Apple signing. | Med | A1: Win64 primary. |
| R12 | Only 11 days remain until Oct 9, and all engine work depends on one workstation. | High | Keep scope to the golden path. Record the video early as a fallback. |
| R9 | Unreal MCP is experimental. Tool names and behavior may change or break mid-project. | Med | D5: CLI evidence is authoritative. MCP is optional acceleration. |
| R8 | The wrap-around Next (F6) breaks predictability. | Low (easy fix) | Stop at the final step and show an explicit “Start again” action. |

## 6. Dependencies / missing inputs
- READY: a UE 5.8.3 workstation (F18). PENDING: Claude Code with Unreal MCP connected there, and the Cesium Fab version recorded.
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
| G1 Versions/license/asset/hardware/deploy known | MOSTLY MET: UE 5.8.3, Cesium latest from Fab, CGTrader model, desktop target, Releases + video. Open: Cesium version number, model license (Q-D), GPU. |
| G2 Model imports and has a perf baseline | WAIVED for implementation start (D17). Proxy stadium. |
| G3 Stadium aligns reproducibly with Cesium | PARTLY WAIVED (D17). Strategy accepted (ADR-0001); calibration happens on the workstation. |
| G4 Four-stop golden path works | OPEN (scaffold partially covers it) |
| G5 Accessibility controls and reduced motion | OPEN |
| G6 Packaged build on target hardware | OPEN — agent can't package (F13) |
| G7 Stakeholder approval | OPEN. Approvers: the user plus the Eagles Autism Foundation. |
