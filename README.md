# eagles-autismfoundation-virtual-stadium

Prototype-friendly Unreal Engine starter project for an interactive **Cesium for Unreal** schedule experience that helps neurodiverse fans prepare for game day with a predictable, step-by-step virtual tour of Lincoln Financial Field.

This first version is intentionally modular and uses placeholder assets only. Any proprietary stadium model, official branding, or venue-specific interior data should be imported later using licensed content.

## What is included

- Unreal 5 C++ starter project scaffold
- Cesium for Unreal plugin registration in `VirtualStadium.uproject`
- Reusable `FStadiumScheduleStop` waypoint/data model
- `UStadiumScheduleDataAsset` for editable, reorderable schedule stops
- `AStadiumTourCamera` for guided fly-through transitions
- Previous / Next / Restart controls with default input mappings
- `UStadiumTourWidget` base class for an accessible UI
- `AStadiumStopMarker` placeholder marker actor for billboards or simple meshes
- Placeholder stop data covering:
  - arrival
  - security
  - entrance
  - concessions
  - sensory room
  - restrooms
  - seating
  - departure
- Manual setup documentation for binary Unreal assets and purchased model import

## Repository structure

```text
.
├── Config/
├── Content/
│   └── Data/
│       └── ScheduleStops.template.json
├── Docs/
│   ├── ImportingStadiumModel.md
│   └── ManualSetup.md
├── Source/
│   └── VirtualStadium/
└── VirtualStadium.uproject
```

## Prerequisites

- Unreal Engine 5.4 or newer
- Cesium for Unreal plugin
- A workstation capable of opening Unreal C++ projects
- Optional: licensed stadium model or neutral placeholder geometry

## Setup

1. Install Unreal Engine 5.4+.
2. Install **Cesium for Unreal**.
3. Open `/home/runner/work/eagles-autismfoundation-virtual-stadium/eagles-autismfoundation-virtual-stadium/VirtualStadium.uproject`.
4. Allow Unreal to build the `VirtualStadium` C++ module.
5. Follow `/home/runner/work/eagles-autismfoundation-virtual-stadium/eagles-autismfoundation-virtual-stadium/Docs/ManualSetup.md` to create the prototype level, data asset, widget blueprint, and marker blueprint.

## Architecture

### Waypoint and schedule model

- `FStadiumScheduleStop`
  - stop id
  - title
  - description
  - world location
  - camera rotation
  - fly duration
  - preview image
  - billboard/placeholder settings

- `UStadiumScheduleDataAsset`
  - stores an ordered array of `FStadiumScheduleStop`
  - lets producers reorder or edit stops without touching code

### Guided navigation

- `AStadiumTourCamera`
  - owns the current stop index
  - interpolates camera movement between waypoints
  - exposes `NextStop`, `PreviousStop`, and `RestartTour`
  - spawns placeholder markers where detailed geometry is missing

### UI

- `UStadiumTourWidget`
  - exposes current stop title, description, preview image, and progress
  - auto-binds optional `PreviousButton`, `NextButton`, and `RestartButton`
  - intended for a Blueprint widget with accessible visual design

## Controls

- `Left Arrow` / `Gamepad D-Pad Left` → previous stop
- `Right Arrow`, `Space`, or `Gamepad D-Pad Right` → next stop
- `R` or `Gamepad Bottom Face Button` → restart tour

## Editing stop order and content

- Update `/home/runner/work/eagles-autismfoundation-virtual-stadium/eagles-autismfoundation-virtual-stadium/Content/Data/ScheduleStops.template.json` as the human-readable source template.
- Mirror those values into the `DA_LFF_ScheduleStops` Unreal Data Asset.
- Reorder the `Stops` array in the Data Asset to change the tour flow.
- Add preview images and neutral placeholder meshes as they become available.

## Cesium integration notes

- Use `CesiumGeoreference` to anchor the venue in real-world coordinates.
- Use `Cesium3DTileset` for terrain, imagery, or surrounding context.
- Keep indoor or proprietary geometry separate from Cesium content so the prototype can evolve incrementally.

## Importing a purchased stadium model

See `/home/runner/work/eagles-autismfoundation-virtual-stadium/eagles-autismfoundation-virtual-stadium/Docs/ImportingStadiumModel.md`.

## Manual Unreal Editor work

The following steps must be done manually in Unreal because `.uasset` files are not stored in this repository:

- Create the prototype map
- Create the schedule Data Asset instance
- Create the Widget Blueprint for the accessible HUD
- Create the marker Blueprint for billboard/model placeholders
- Assign preview images, sprites, and meshes
- Import and align any licensed stadium model

## Next steps

- Add calm audio narration and optional captions
- Replace placeholders with licensed stadium geometry
- Add alternate routes for quieter entry or exit flows
- Add platform-specific accessibility testing in Unreal
- Introduce analytics or feedback capture only after privacy review
