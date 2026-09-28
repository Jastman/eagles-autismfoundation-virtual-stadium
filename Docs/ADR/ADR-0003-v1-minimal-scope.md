# ADR-0003: V1 minimal golden-path scope (red-team outcome)

- **Status:** Accepted
- **Date:** 2026-09-28
- **Amends:** ADR-0001 (placement storage) and ADR-0002 (class count, settings, inactivity, input, video)

## Context
ADR-0002 planned about 17 classes, 7 widget Blueprints and 11 input assets. The code is written in a cloud container that has no compiler, and the engine workstation fixes compile errors one round trip at a time. There are 11 days until Oct 9, and about $33 of the budget is left for implementation. The red-team review found critical packaging gaps (no default map, no cook rules for soft-loaded assets), a startup-order bug, timed interactions that contradict ACCESSIBILITY.md, and user decisions missing from the schema (intro screen, Calm Mode).

## Decision
Build the golden path with **8 classes plus 1 test file**:

| Class | Role |
|---|---|
| `LFFJourneyTypes.h` | Step row, enums, visitor settings, snapshot (header only) |
| `FLFFJourneyStateMachine` + `LFFJourneyRules` | Pure C++ state: Intro → steps, no wrap, pause, transition ids |
| `ULFFJourneySubsystem` (WorldSubsystem) | Loads the table, finds targets by iterator on the tick after BeginPlay, spawns the rig if the level has none, owns settings, modals, labels and audio, and broadcasts one snapshot |
| `ALFFCameraRig` | Ease, fade or instant camera moves. Ticks only while easing. Completions are guarded by transition id |
| `ALFFCameraTarget` | A named pose (`TargetId`, FOV) placed under `LFF_SiteRoot` |
| `ALFFGameMode` | Config and copy defaults in C++. No pawn. The table is soft-loaded from a fixed path |
| `ALFFPlayerController` | **Input actions and key mappings built in code** (no input assets). Loads the HUD from a fixed path |
| `ULFFHUDWidget` | The only C++ widget. Required bindings for the disclaimer, the prototype badge, the title and the body |
| `LFFJourneyStateMachineTest.cpp` | Headless automation tests |

Behaviour decisions:
1. **Packaging.** `DefaultEngine.ini` sets `GameDefaultMap`, `EditorStartupMap` and `GlobalDefaultGameMode`. `DefaultGame.ini` cooks `/Game/LFF/Levels/L_LFF_Stadium`, `/Game/LFF/Data` and `/Game/LFF/UI`.
2. **Example ticket.** The seat text is written into the step JSON (Section 111, Row 10, Seat 8). There's no runtime JSON read and no token substitution. Copy never says "scan". The badge on every screen reads "Prototype · Illustrative · Example ticket".
3. **All steps are Illustrative.** There's one disclaimer string for V1. The banner can't be hidden, because C++ re-shows it after the Blueprint event.
4. **Intro state.** It's shown on the first step's view, so Start never moves the camera. The end of the journey offers "Start again", which leads to the intro with a calm fade.
5. **Restart asks first** ("Start again?" Yes/No). There's no hold-to-restart and no timed interaction.
6. **Inactivity reset is cut from V1** and moved to Tier B. This is a desktop app used by families, so a timeout could throw away a slow reader's place.
7. **Settings persist for the session** and are kept across restart. They reset only when the app launches. Nothing is written to disk.
8. **Calm Mode** is opt-in. It turns on reduced motion, multiplies sound volume by 0.5, and the Blueprint applies a muted theme via `OnSnapshotChanged`.
9. **Keyboard shortcuts are the main keyboard path.** HUD buttons hand focus back to the viewport after every click, so Tab focus navigation isn't supported in V1 (see KNOWN_LIMITATIONS). Controllers can use Next, Back, Pause, Settings, Restart, Confirm and Cancel. The settings toggles need a keyboard (1–4) or a mouse.
10. **Video.** There's no in-app RecordMode. An operator follows a shot list and captures a packaged Development build with OBS, keeping the badge and the Cesium/Google credits visible. The first take is due by Oct 5.
11. **Placement (amends ADR-0001).** For V1 the georeference origin and site-root values live in the map and are copied into the table in `CESIUM_PLACEMENT.md`. `DA_LFF_GeoPlacement` and its Apply/Validate tooling are deferred.

Cut from V1: audio director class, label marker class (labels are any actors tagged `LFF_Label` plus a label id), kiosk settings, settings subsystem, inactivity tracker, validation class, extra C++ widgets, debug commands, data drift test, geo placement data asset.

## Alternatives
- Keep the ADR-0002 set. Rejected: too much uncompiled code for the time and budget.
- Build input assets in the editor. Rejected: 12 more binary assets, and the bindings couldn't be reviewed in git. Code-built mappings are **VERIFY** on 5.8. The fallback is to author an IMC asset with the same keys.

## Consequences
- Less editor work: one map, one Data Table, one widget Blueprint.
- Adding inactivity reset, label markers or geo tooling later means new classes, but the snapshot and command API stay the same.

## Validation
1. The editor target compiles on the workstation (Mac and Windows).
2. `Automation RunTests LFF.` passes headless.
3. The PIE checklist in `Docs/TEST_PLAN.md` passes.
4. A packaged build launches straight into the journey.
