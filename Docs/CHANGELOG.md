# Changelog

## 2026-10-01 (session 5: lot grid, MetaHuman crowd, voice on by default, karaoke)
- **Lot on its own grid** (`Tools/Pipeline/plan_lot.py`, `Tools/Editor/session5_assets.py`): the model's lot is laid out 16.5 deg off the stadium frame (measured from its walkway islands and edges), so the bays now follow it: 2.7 m stalls, 7.2 m aisles, cross aisles. 811 instanced City Sample cars sit square in their stalls on 1,437 painted lines (`SI_LotPaint`, `M_LFF_LotPaint`). The Pepsi Plaza entry keeps 10 m clear and its five cameras' views; no car touches the line or security.
- **Parking step faces the stadium:** `CT_Parking` stands in an aisle looking south down the rows, the ten hero cars either side and the arriving car pulling into the empty bay on the right; the stadium fills the background.
- **MetaHuman Crowd Sample crowd** (Mass): `ALFFStadiumCrowdSpawner` + `ULFFSeatSpawnDataGenerator` place the sample's people at the stadium's own seats: 56,707 seats read from the model's seat parts (`Tools/Pipeline/extract_seats.py`, `Content/LFF/Crowd/seats.json`), 70% occupied (39,695 people) across every section, lower to upper deck, each facing the field; the seats around the camera seat are left empty. `DA_LFF_StadiumCrowd` is the sample's base config without walking, steering and avoidance, instanced skinned meshes at every LOD out to 600 m. Crowd Off and Calm Mode despawn it.
- **The queue is full:** 64 MetaHumans in the three stanchion lanes and a tail beyond them (`SI_QueueCrowd`, `queue.json`); the six primitive queue figures are hidden.
- **Sound and Voice on by default.** Each card has a voice icon button (top right, speaker / speaker crossed out, 64 px target); it turns Sound on with the voice if needed. Calm Mode still silences both.
- **Karaoke:** while the voice reads, the card highlights the word being spoken (warm gold, colour only). Word timings are aligned locally from the script and each wav's pauses (`Tools/Audio/make_vo_words.py`) and stored as the voice SoundWaves' subtitle cues (suppressed on screen); the card's title and body are rich text.
- **Finale card:** the wordmark is a UI texture (sharp), under the body text; Learn More no longer overlaps Start Over.
- **No engine debug text on screen** in play (texture pool, shadow cache); texture streaming pool 4 GB.
- Dev: `LFF.Shots <steps> [quit]` screenshots journey steps offscreen (not in Shipping).

## 2026-09-30 (session 4 finish pass: `Tools/Editor/finish_pass.py`)
- **No editor icons in any view:** the billboard on every plain actor, the "Tt" sprite on every TextRenderActor and the camera targets' arrows are hidden (they were already hidden in game; now also in the editor viewport and captures).
- **Ticket:** the phone's header reads "Eagles vs. Carolina Panthers / Sun, October 18 · 1:00 PM" (was "Eagles vs. Lions"), set in the same face on the same flat bar; seat 111 / 10 / 8 as before. 30 frames re-imported.
- **Parking:** `CT_Parking` 4 m further back, 4 m up, 2 degrees right and 7 degrees down, so all 11 City Sample cars are in frame (back row over the front row), clear of the caption card.
- **The whole lot is full:** `SI_LotCars`, 882 cars: 5 City Sample models in 8 paints as instanced static meshes (a body per model and paint, wheels and glass per model), on every asphalt bay of the model's lot around the stadium (found from a top-down capture, `Saved/LotMask`), in double rows that carry on from the hero pair (elsewhere 2.7 m bays, 7.2 m aisles, a cross aisle every 20 bays). The arriving car's lane, each step camera's near view, the stand-ins and the hero rows' sight lines stay clear; no cars on the thin perimeter road (strips under 12 m wide). Every car is traced onto the lot.
- **The lot unloads inside the stadium:** actors tagged `LFF_Exterior` (the lot, the ten parked hero cars, the bay lines) are hidden while the live camera is inside the `LFF_StadiumInterior` box (`ULFFJourneySubsystem::UpdateExteriorVisibility`, checked 5 times a second; logged "Camera inside/outside the stadium: N exterior actors hidden/shown").
- **Plaza people hidden:** the nine primitive plaza, popcorn and sensory-trailer people read as grey-headed bollards from those cameras, so they're hidden in game (the line, security and staff figures stay).
- **Bowl crowd:** `SI_BowlCrowd`, 198 upright crowd cards (one row of 16 painted seated fans each, midnight green / white / silver / black; `Tools/Pipeline/prepare_crowd.py`) on five front rows of both sidelines and the north end (row 4 onward), as two instanced meshes; `M_LFF_StadiumMasked` now has its instanced-mesh usage flag. Tagged `LFF_Crowd`, so Crowd Off and Calm Mode hide them. No AI, Mass or animation.
- **Finale card:** the Eagles Autism Foundation wordmark (`Assets/Logos`) on a small bone plate under the body text, last step only, next to Learn More. Built by `ULFFJourneyPanel` when the blueprint has no `FinaleLogo` widget.
- **Step 1 card** is 90% size (`FirstStepCardScale`), so it stays off the stadium in the wide aerial.
- **Audio checked, unchanged:** all 23 voice assets are the ElevenLabs wavs (MD5 match, full-band); the seat bed is the steady Ambience loop (2.4 dB range over the loop and its seam).

## 2026-09-30 (session 4: voice, logos, cars, cheer)
- **Voiceover:** the 23 lines are ElevenLabs text-to-speech (voice Sarah, `EXAVITQu4vr4xnSDxMaL`, `eleven_multilingual_v2`), generated outside Unreal and mastered to -20 LUFS, 48 kHz mono, 0.5 s head / 0.8 s tail. Same filenames; imported with `import_lff_vo.py`. The recipe is in `VO_MANIFEST.md` (`make_vo_script.py`).
- **Ticket:** `prepare_ticket.py` fills the GIF's 107 / 18 / 22 from the surrounding background and sets 111 / 10 / 8 in their place on all 30 frames.
- **Logos** (`Tools/Pipeline/prepare_logos.py`, `import_lff_content.py` step `logos`, `Assets/Logos` only): the Eagles head as a decal at midfield (`SI_MidfieldLogo`, `M_LFF_LogoDecal`), seen from every seat step and the finale; Wawa and EAF logos on the popcorn cart header, under the title.
- **Cars:** the 10 parked cars and the arriving car are City Sample Vehicles (five everyday models, varied paint via `MI_LFF_CarPaint_*`; no trucks, garbage truck, vans, bus, taxi or police car). Static meshes, nothing simulates.
- **Cheer bed:** `SW_LFF_CheerBed` is now cut from the Free Crowd Cheering Sounds pack (`Tools/Editor/export_cheer_pack.py`, `Tools/Audio/make_cheer_bed.py`), steady, at the old bed's level; `make_lff_audio.py` only synthesizes it with `--synth-cheer`.
- **Import:** `import_lff_content.py` steps can be chosen with `LFF_CONTENT_STEPS`.
- **MetaHuman crowd:** plugin enabled; blocked on the missing MetaHumanCharacter optional content (KNOWN_LIMITATIONS). The primitive crowd stays.

## 2026-09-30 (session 3: cinematic layer, hero stand-ins, the Oct 18 1:00 PM timeline, voiceover)
- **Journey data: the Oct 18, 2026 example game** (Panthers at Eagles, kickoff 1:00 PM ET). `LFF_GoldenPath.json` now holds the brief's 23 steps (02–24; the splash is 01), in order, word for word. Timed steps carry `ClockText` and are `Approximate` (a typical 1:00 PM Linc pattern, not confirmed operations); the rest stay `Illustrative`. The sensory and Guest Services steps are kept.
- **Voiceover.** New `FLFFJourneyStep::VoiceWave` and the Voice setting (Off by default, key 5, Settings). The card is the script and the caption: the voice reads exactly Title then Body, once, 0.5 s after the camera arrives; a step change stops it at once; the bed ducks 8 dB; Sound Off or Calm Mode silences it; previews stop it; no voice on the splash; a missing wave is skipped. `Tools/Audio/make_vo_script.py` writes `SourceData/Audio/VO/VO_MANIFEST.md`, one `.txt` per line and (with `--placeholders`) macOS `say` test voices; `Tools/Editor/import_lff_vo.py` imports `VO_*.wav` to `/Game/LFF/Audio/VO` (non-looping). Unreal never calls a voice service.
- **Camera spine.** The splash slowly orbits the stadium (40 s a lap) through the new `CT_StadiumWide` (stadium, sports complex and Center City); Less movement or Calm Mode holds a still wide shot. Start glides along the orbit into that shot; parking follows. `ALFFCameraRig::StartOrbit`, `EaseOrbitTo`, `ClearFade` (a replaced fade can't leave the picture dim).
- **Timeline** above Back/Next: six clickable marks (10:00 Plaza gates, 11:00 Club gates, 11:30 Inner gates, 11:50/12:00/12:15 Flares, 12:50 Anthem, 1:00 Kickoff), "Approximate times · Example 1:00 PM game"; only the current step's mark is emphasized. `FLFFJourneyStep::TimelineLabel`, `FLFFJourneyStateMachine::JumpTo`, `ULFFJourneySubsystem::JumpToStep`.
- **Scene cues** (`ALFFCueActor`, `SceneCues`): the Pepsi Plaza, club and inner gates slide open on their steps; one car rolls into the lot on parking; Less movement and Calm Mode show them already open or parked. The anthem preview (`ELFFIntensityPreview::Anthem`) flies the bald eagle across the field once and plays soft fireworks (`ALFFFireworksActor`, steady glow, no flashes); Calm Mode hides it.
- **Hero stand-ins** (`build_lff_standins.py`): the Popcorn for the People cart (white cart, black-and-white striped awning, red popcorn-box wall, "Wawa | POPCORN FOR THE PEOPLE | Eagles Autism Foundation"), the Sensory Activation Vehicle trailer (black, green SENSORY ACTIVATION VEHICLE / MOBILE SENSORY SPACE, KultureCity), three gate proxies, a new Pepsi Plaza entry (line, security, gate, bag check, ticket staff) between the lot and the plaza, 10 parked cars and 15 plain crowd stand-ins (Crowd Low/Off, key 6). The standing phone board is gone.
- **Mobile ticket:** a phone rises from the bottom of the screen showing the ticket GIF's frames (`Tools/Pipeline/prepare_ticket.py`); Calm Mode and Less movement show it already up and still.
- **Eagle:** `Tools/Pipeline/prepare_eagle.py` bakes the glTF's flap loop to Alembic in Blender; imported as `GC_LFF_BaldEagle` (GeometryCache) with its textures.
- **Beds:** a synthesized soft cheer (`SW_LFF_CheerBed`) on the seat, kickoff and finale steps; the plaza murmur elsewhere; beds cross-fade.
- **Finale:** "Start Over" (splash, voice stopped) and "Learn More" (the Eagles Autism Foundation page). No logo (none in the project).
- **HUD** extended in place, not rebuilt (`build_wbp_lff_ui.py --extend`): Voice and Crowd toggles and keys 5/6 in Settings; timeline and Learn More on the rail; the ticket phone on the caption layer; the caption card moved up above the timeline.
- **Fab packs not imported:** City Sample Vehicles, MetaHuman Crowd Sample and Free Crowd Cheering Sounds weren't on the machine; primitives and synthesized sound stand in (KNOWN_LIMITATIONS).
- Tests: `LFF.Journey.StateMachine.Jump`, and `Rules` now covers voice/sound/calm, crowd, cue states, the splash orbit and the anthem preview. 5/5 pass.

## 2026-09-29 (session 2: smaller HUD, working Sound and Calm Mode, the EAF schedule)
- **Smaller HUD, same panels.** `build_wbp_lff_ui.py --rebuild` edits the five panels in place. The caption card is about 450 px wide at 1080p (it was about 720), title 30 / body 22 (were 48 / 32), the top bar and rail are 64 px (were 72 / 88), and Start is 72 px (was 96). Hit targets stay at 64 px or more. The Settings, Paused and "Start again?" cards shrank to match.
- **Disclaimer line.** One small persistent line under the badge on journey steps, "Illustration. Not directions for the real stadium." (`DisclaimerLineText`, required by `ULFFHUDWidget`). The chip on the card stays.
- **World labels** sit above their props. At runtime a label that would land in the lower third of its step's framing is hidden (`LFFJourneyRules::ShouldHideLabelAt`, tested). `LBL_Plaza` moved up above the plaza; `LBL_Food` moved off the wall.
- **Sound works.** Off mutes the whole audio device (UI included). On plays one low synthesized plaza murmur (`/Game/LFF/Audio/SW_LFF_AmbientPlaza`, made by the new `Tools/Audio/make_lff_audio.py`, imported by the new `Tools/Editor/import_lff_content.py`). There were no sound assets before, which is why Sound did nothing.
- **Calm Mode works.** Less movement on, Sound off, `LFF_Crowd` actors hidden, previews locked off, camera cuts only (`LFFJourneyRules::ResolveMove`, tested). The Sound and Less movement toggles show the effective state and are disabled while it's on. `CalmVolumeMultiplier` was removed.
- **Intensity previews** on steps 11–12 (`FLFFJourneyStep::IntensityPreview`): "Preview this? Off / Soft" (button or V), never autoplayed. Sound: an 8 s muffled sample behind the button. Lights: a slow exposure pulse on the camera (`ALFFCameraRig::SetLightPulse`, 4 s cycle, +0.9 stops max, no flicker).
- **No wall clipping.** An Ease whose straight path sweeps into geometry fades and cuts instead.
- **Journey data:** the 4 generic steps are replaced by the 16 Eagles Autism Foundation visual-schedule beats, first person, short titles, all Illustrative.
- **Stand-ins and targets:** the new `Tools/Editor/build_lff_standins.py` (MCP, rerunnable) adds 11 `CT_` targets and cheap props: 3 cars, stanchions, security arches, a clear-bag table, a phone ticket, a popcorn kiosk, a van, a Guest Services booth, a sensory-room portal, a food counter and a staff mannequin, each with an "(illustration)" label. The original 4 targets are unchanged. No Cesium, origin or stadium-import changes.
- **Evidence logging:** `LogVirtualStadium` now logs each camera move's profile, Sound on/off, the bed and preview starting and finishing, the Lights pulse, and Calm Mode.

## 2026-09-29
- **HUD restyle (LEDGER D26).**
  - `WBP_LFF_HUD` now holds five panels, built by the new `Tools/Editor/build_wbp_lff_ui.py`, which replaces `build_wbp_lff_hud.py`:
    - `WBP_LFF_Splash`: centre-low welcome card with **Start Your Day**.
    - `WBP_LFF_TopChrome`: badge, plus Step X of Y · Pause · Sound · Settings.
    - `WBP_LFF_StepCaption`: lower-left card with a disclaimer chip.
    - `WBP_LFF_BottomRail`: Back · pips · Next.
    - `WBP_LFF_Settings`: modal layer over a dimmed world, with toggles, key legend, Start again, Paused and "Start again?".
  - New C++ parent `ULFFJourneyPanel` for the panels. `ULFFHUDWidget` now composes the panels and fades them in (0.25 s ease-out; instant with reduced motion or Calm Mode).
  - Journey: Back on step 1 returns to the splash (`LFFJourneyRules::ShouldBackReturnToIntro`, tested). "Start again" from Settings replaces the panel with the question. Added the `bHighContrast` setting and `ToggleHighContrast`. `ShouldShowToggles` was removed.
  - Splash copy: "Welcome to / Lincoln Financial Field / A calm preview of game day. You choose when to move on." Step content is unchanged.
  - Docs: JOURNEY_SCHEMA "HUD widgets", ManualSetup §5, LOCAL_AGENT_SETUP §5, TEST_PLAN T2 rows 1, 2, 6, 11 and 15, and LEDGER D18 and D26.
- **Sky and time of day.**
  - The sun is at 1:00 pm local on Oct 18, 2026 (EDT, via TimeZone −5 plus CesiumSunSky DST). This fixes the earlier −4, which was an hour late.
  - Partly cloudy volumetric sky (`MI_LFF_Clouds_PartlyCloudy`): static, no lightning, half samples, no ground cloud shadows.
  - `setup_cinematic_look.py` and `build_lff_stadium_level.py` updated to match.
  - 60 fps is kept (PERF_BASELINE).
- **Git LFS** now tracks `Content/LFF/Stadium/**` (`.gitattributes`).
- **Official stadium model integrated (ADR-0004).**
  - Blender pipeline `Tools/Pipeline/prepare_stadium.py`: OBJ in, NFL field, 26 chunks, tile registration.
  - Unreal import `Tools/Editor/import_stadium.py`: Nanite, PBR materials, the tile clip polygon and overlay, the proxy concourse removed.
  - Also added `setup_cinematic_look.py`, `sample_tile_dsm.py`, `run_headless.sh` and `probe_views.py`.
  - The concourse target and its signs moved to the model's plaza-level concourse.
- **Cinematic rendering.**
  - `DefaultEngine.ini` now enables Lumen GI and reflections, VSM, distance fields, no static lighting, TSR and `CachedLightingPreExposure=8`.
  - `PPV_LFF_Cinematic`, the aerial fog and the sun at 3:30 pm Oct 9 are set.
  - The turf material is toned down.
- **Performance:** 60 fps at 1080p on the M4 Max (`Docs/PERF_BASELINE.md`).
- `Assets/` is git-ignored. Updated LEDGER F40–F42, D25 and G2, and ASSET_INGESTION.

## 2026-09-28
- **Legible labels.**
  - Each label is now white text on a dark sign plate (`MI_LFF_SignPlate`, a component of the label actor), created by `build_lff_stadium_level.py`.
  - The gate sign is 150 → 200 cm, and the restroom and food signs are 40 → 50 cm.
  - The section sign is moved 3 m ahead into the lower third and resized to 12 cm.
  - Removed the seat "Illustration" marker (its proxies are gone).
  - Headless runs now use `-ModelContextProtocolPort=18000` (F39).
  - Logged the concourse tile intrusion and a proposed fix as an open item.
- **Tiles render and the site is calibrated.**
  - Added `r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True` to `DefaultEngine.ini`. Without it CesiumSunSky rendered everything white.
  - Applied placement revision 2026-09-28a (origin 39.9008036, -75.1674790, -31.04 m; SiteRoot -6.5°), measured from the tiles with overhead captures and line traces.
  - Re-posed CT_GateEntry and CT_SeatFieldView and their labels, and removed the proxy seat rows.
  - `build_lff_stadium_level.py` carries the same values.
  - Docs: CESIUM_PLACEMENT, LEDGER F37–F38 and G3, LOCAL_AGENT_SETUP §5 gotchas.
- **One set of toggles (C++ plus HUD).**
  - Added the optional `TogglesPanel` binding to `ULFFHUDWidget`, with `LFFJourneyRules::ShouldShowToggles` and tests.
  - `WBP_LFF_HUD` now shows Less movement, Calm Mode and Large text on the intro *and* in Settings. The intro and Settings headers sit above them, with Close in the Settings title bar.
  - Updated JOURNEY_SCHEMA, ManualSetup §5 and the LEDGER.
- **Scripted level and HUD (Mac).**
  - Added `Tools/Editor/build_lff_stadium_level.py`: a headless editor script that imports `DT_LFF_GoldenPath` from the JSON and builds `L_LFF_Stadium`. The level has the georeference (tagged `DEFAULT_GEOREFERENCE`), the Google tileset, the sun, and `LFF_SiteRoot` with its anchor. Under the root sit the 4 `CT_` targets, the labels, and the concourse and seat proxies. The script validates the structure before saving.
  - Added `Tools/Editor/build_wbp_lff_hud.py` and `Tools/Editor/unreal_mcp.py`, which build `WBP_LFF_HUD` through Unreal MCP.
  - PIE reaches the intro.
  - Stopped tracking `Content/CesiumSettings/CesiumIonServers/` and ignored it, so the ion token can't be committed (D23).
  - Docs: LOCAL_AGENT_SETUP §5, the ManualSetup automation note, the CESIUM_PLACEMENT seed values and token location, and LEDGER F35–F36, D23–D24 and open items.
- **First workstation build (Mac).**
  - Moved `VirtualStadium.Target.cs` and `VirtualStadiumEditor.Target.cs` from `Source/VirtualStadium/` to `Source/`, and switched both to `BuildSettingsVersion.Latest`.
  - The editor target now compiles with 0 warnings, and the 4 `LFF.*` tests pass headless (T1.1 and T1.2).
  - Verified the UE, Cesium and Unreal MCP details in `LOCAL_AGENT_SETUP.md` §1–2 and the ledger (F30–F33).
  - Ignored the Cesium request cache and `.DS_Store`.
- **Unreal MCP connected (Mac).**
  - Enabled Unreal MCP, Terminal, EditorToolset, UMG ToolSet and AutomationTestToolset in the editor.
  - Committed the generated `.mcp.json`, `Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset`, and the engine-generated Android File Server block in `DefaultEngine.ini` (committed once so the editor stops re-adding it).
  - The smoke test passed (LEDGER F34).
  - Documented the `call_tool` calling convention and the approval risk in `LOCAL_AGENT_SETUP.md`.
- Phase 0 started. Added `Docs/LEDGER.md`, `Docs/USAGE_LEDGER.md`, `Docs/CHANGELOG.md` and `Docs/ADR/`. No code or config changes.
- Retargeted the project to UE 5.8 (`VirtualStadium.uproject`, README and ManualSetup prerequisites) at the user's direction.
- Added `Docs/LOCAL_AGENT_SETUP.md`, which covers the Unreal MCP connection, CLI build and package commands, and 5.8 migration watch items.
- Ledger: added F15–F17, D3–D5 and R9.
- Added `Docs/AGENT_TOOLING.md`: inventory of the requested Unreal and UI skills and MCPs, local install steps, and trust rules. None could be installed from the cloud session (egress/scope blocked).
- Added ADR-0001 (geospatial placement, Proposed) and `Docs/CESIUM_PLACEMENT.md`: fixed cartographic origin, single anchored site root, no origin shifting, polygon clipping, placement kept in `DA_LFF_GeoPlacement`, steps reference targets only by `TargetId`.
- Lead synthesis: recorded the user's answers (F18–F25) and decisions D6–D17. Reconciled names (ALFFCameraTarget, LFF_SiteRoot, FLFFVisitorSettings), set inactivity to 120/30/300 s and no settings persistence, formally waived G2 and G3 (partly) to start implementation. Retargeted docs to UE 5.8.3 and Cesium from Fab.
- **Golden-path code (not compiled yet):**
  - Replaced the scaffold classes with the 8-class V1 set from ADR-0003: state machine, journey subsystem, camera rig and target, game mode, player controller with code-built Enhanced Input, and HUD widget base. Added headless tests.
  - Removed `StadiumScheduleDataAsset`, `StadiumTourCamera`, `StadiumStopMarker` and `StadiumTourWidget`.
- **Data:** added `SourceData/Journey/LFF_GoldenPath.json` (4 illustrative steps, example seat). Moved the old 8-stop template to `SourceData/Journey/LFF_Backlog_LegacyStops.json`.
- **Config:** default map and game mode, Enhanced Input classes, and cook rules for `/Game/LFF/{Levels,Data,UI}`.
- **Docs:**
  - Added ADR-0003, `TEST_PLAN.md` and `KNOWN_LIMITATIONS.md`.
  - Rewrote `JOURNEY_SCHEMA.md` and `ManualSetup.md`.
  - Added Mac and Windows CLI and release steps to `LOCAL_AGENT_SETUP.md`, and Google Photorealistic 3D Tiles notes to `CESIUM_PLACEMENT.md`.
  - Ledger: F26–F29 and D18–D22; revised D1, D2, D10, D11, D12, D14 and D15.
