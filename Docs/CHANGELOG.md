# Changelog

## 2026-09-28
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
