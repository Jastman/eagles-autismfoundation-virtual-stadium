# eagles-autismfoundation-virtual-stadium

Prototype-friendly Unreal Engine starter project for an interactive **Cesium for Unreal** schedule experience that helps neurodiverse fans prepare for game day with a predictable, step-by-step virtual tour of Lincoln Financial Field.

Virtual schedule: https://static.clubs.nfl.com/image/upload/eagles/f1h4ec616hii4k9yqeay

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
- Placeholder stop data template covering:
  - arrival
  - security
  - entrance
  - concessions
  - sensory room
  - restrooms
  - seating
  - departure
- Manual step to copy the included template data into an Unreal `StadiumScheduleDataAsset`
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
3. Open `VirtualStadium.uproject`.
4. Allow Unreal to build the `VirtualStadium` C++ module.
5. Follow `Docs/ManualSetup.md` to create the prototype level, data asset, widget blueprint, and marker blueprint.
6. Keep Unreal's legacy input mappings enabled, or mirror the `TourPrevious`, `TourNext`, and `TourRestart` actions into an Enhanced Input mapping context before testing controls.

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

This starter binds legacy `ActionMappings` from `Config/DefaultInput.ini`.

If the project was already open when these mappings were added or pulled, restart the editor or reload input settings before testing the controls.

- `Left Arrow` / `Gamepad D-Pad Left` → previous stop
- `Right Arrow`, `Space`, or `Gamepad D-Pad Right` → next stop
- `R` or `Gamepad Bottom Face Button` → restart tour

## Editing stop order and content

- Update `Content/Data/ScheduleStops.template.json` as the human-readable source template.
- Mirror those values into the `DA_LFF_ScheduleStops` Unreal Data Asset.
- Reorder the `Stops` array in the Data Asset to change the tour flow.
- Add preview images and neutral placeholder meshes as they become available.

## Cesium integration notes

- Use `CesiumGeoreference` to anchor the venue in real-world coordinates.
- Use `Cesium3DTileset` for terrain, imagery, or surrounding context.
- Keep indoor or proprietary geometry separate from Cesium content so the prototype can evolve incrementally.

## Importing a purchased stadium model

See `Docs/ImportingStadiumModel.md`.

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
# Eagles Autism Virtual Stadium
<img width="1376" height="768" alt="image_1790610769396212" src="https://github.com/user-attachments/assets/c301bbdd-c066-45df-b0b3-bd9e1284f987" />

An interactive Cesium for Unreal experience designed to help neurodiverse fans and their families prepare for game day at Lincoln Financial Field.

Overview

This project transforms the Philadelphia Eagles’ existing virtual schedule into a guided 3D stadium experience. Fans can preview key stages of their visit—from arrival and stadium entry to concessions, sensory spaces, seating, and departure—in a predictable, accessible format.

Goals

• Reduce uncertainty and anxiety before game day
• Provide a guided camera fly-through of the fan journey
• Highlight important locations, including concessions, restrooms, seating, and sensory spaces
• Demonstrate the potential of Cesium and Unreal Engine for accessible sports experiences

Technology

• Unreal Engine
• Cesium for Unreal
• 3D model of Lincoln Financial Field

Initial Prototype

The first version will:

• Recreate the existing virtual schedule as an interactive 3D sequence
• Guide users through stadium locations step by step
• Use billboards or simple models where detailed interior scans are unavailable
• Add crowds, screen animations, and other game-day elements where practical
• Run as an interactive application on a large display

Future Vision

Future versions could include:

• Ticket-specific routes from parking lot to seat
• Directions to nearby restrooms, concessions, and sensory spaces
• QR-code access from a fan’s ticket
• Sound and crowd-density previews
• Detailed interior scans
• Personalized accessibility settings
• VR support

Status

Early prototype in development for the Eagles Autism Foundation partnership.
