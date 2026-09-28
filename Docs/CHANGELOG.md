# Changelog

## 2026-09-28
- Phase 0 started. Added `Docs/LEDGER.md`, `Docs/USAGE_LEDGER.md`, `Docs/CHANGELOG.md` and `Docs/ADR/`. No code or config changes.
- Retargeted the project to UE 5.8 (`VirtualStadium.uproject`, README and ManualSetup prerequisites) at the user's direction.
- Added `Docs/LOCAL_AGENT_SETUP.md`, which covers the Unreal MCP connection, CLI build and package commands, and 5.8 migration watch items.
- Ledger: added F15–F17, D3–D5 and R9.
- Added `Docs/AGENT_TOOLING.md`: inventory of the requested Unreal and UI skills and MCPs, local install steps, and trust rules. None could be installed from the cloud session (egress/scope blocked).
- Added ADR-0001 (geospatial placement, Proposed) and `Docs/CESIUM_PLACEMENT.md`: fixed cartographic origin, single anchored site root, no origin shifting, polygon clipping, placement kept in `DA_LFF_GeoPlacement`, steps reference targets only by `TargetId`.
