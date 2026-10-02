# Eagles Autism Virtual Stadium
<img width="1376" height="768" alt="image_1790610769396212" src="https://github.com/user-attachments/assets/f91cdf2c-bd63-4058-89f6-dfe28c8404d3" />

An interactive Cesium for Unreal experience designed to help neurodiverse fans and their families prepare for game day at Lincoln Financial Field.

## Overview

This project transforms the Philadelphia Eagles’ existing virtual schedule into a guided 3D stadium experience. Fans can preview key stages of their visit—from arrival and stadium entry to concessions, sensory spaces, seating, and departure—in a predictable, accessible format.

## Goals

• Reduce uncertainty and anxiety before game day
• Provide a guided camera fly-through of the fan journey
• Highlight important locations, including concessions, restrooms, seating, and sensory spaces
• Demonstrate the potential of Cesium and Unreal Engine for accessible sports experiences

## Technology
- Unreal Engine
- Cesium for Unreal
- [3D model of Lincoln Financial Field](https://www.cgtrader.com/3d-models/exterior/stadium/lincoln-financial-field-philadelphia-usa)

## Initial Prototype

The first version will:

• Recreate the existing virtual schedule as an interactive 3D sequence
• Guide users through stadium locations step by step
• Use billboards or simple models where detailed interior scans are unavailable
• Add crowds, screen animations, and other game-day elements where practical
• Run as an interactive application on a large display

## Future Vision

Future versions could include:

• Ticket-specific routes from parking lot to seat
• Directions to nearby restrooms, concessions, and sensory spaces
• QR-code access from a fan’s ticket
• Sound and crowd-density previews
• Detailed interior scans
• Personalized accessibility settings
• VR support

## Status

Early prototype in development for the Eagles Autism Foundation partnership.

## What is included (V1 golden path)

- A UE 5.8.3 C++ module (`Source/VirtualStadium`) with 8 small classes:
  - a pure journey state machine (no wrap, pause, intro);
  - a world subsystem that runs the journey;
  - a camera rig with eased, reduced-motion (fade) and instant moves;
  - named camera targets;
  - a game mode and a player controller (Enhanced Input built in code);
  - one HUD widget base class that always shows the disclaimer.
- Headless automation tests: `LFF.Journey.*`.
- Sixteen illustrative steps in `SourceData/Journey/LFF_GoldenPath.json`, following the Eagles Autism Foundation visual schedule: arriving, parking, the line, security, clear bags, the mobile ticket, the plaza, Popcorn for the People, the Sensory Activation Vehicle, walking to the seat, loud sounds and bright lights (optional soft previews), Guest Services, the sensory room, food and staff. Simple stand-in props mark each stop.
- Cesium for Unreal v2.29.1 with Google Photorealistic 3D Tiles for the context.
- Project docs in `Docs/`: ledger, ADRs, setup, test plan, accessibility rules, wording and known limitations.

## Repository structure

```text
.
├── Config/                     engine, input and packaging settings
├── Docs/                       LEDGER, ADR/, setup, schema, test plan, accessibility
├── SourceData/Journey/         step data (JSON; imported into DT_LFF_GoldenPath)
├── Source/VirtualStadium/      C++ module
└── VirtualStadium.uproject
```

## Prerequisites

- Unreal Engine 5.8.3 (see `Docs/LOCAL_AGENT_SETUP.md`)
- Cesium for Unreal v2.29.1 from [Fab](https://www.fab.com/listings/76c295fe-0dc6-4fd6-8319-e9833be427cd), plus a Cesium ion account with Google Photorealistic 3D Tiles
- macOS with Xcode, or Windows with Visual Studio 2022. Each platform is packaged on its own OS.
- An internet connection at runtime (Google tiles stream live)

## Setup

1. Build and run the tests (`Docs/LOCAL_AGENT_SETUP.md` §3).
2. Follow `Docs/ManualSetup.md` to create the map, import the Data Table, place the camera targets and build `WBP_LFF_HUD`.
3. Check the result against `Docs/TEST_PLAN.md`.

## Controls

| Action | Keyboard | Controller |
|---|---|---|
| Next / Start | Right, Space, PageDown | D-pad Right, RB |
| Back | Left, Backspace, PageUp | D-pad Left, LB |
| Yes (else Next) | Enter | A |
| No / close / resume | Esc | B |
| Pause | P | Menu |
| Settings | S | View |
| Start again? | R | Y |
| Less movement / Sound / Calm Mode / Large text | 1 / 2 / 3 / 4 | — |

## Editing steps

Edit `SourceData/Journey/LFF_GoldenPath.json`, open a PR, then reimport `DT_LFF_GoldenPath` in the editor. Steps name a camera target (`CameraTargetId`) and never hold coordinates. Every V1 step is `Illustrative` and shows the disclaimer.

## Status

Prototype for the Eagles Autism Foundation partnership. The code is written but not yet compiled. See `Docs/LEDGER.md` for gates, decisions and blockers, and `Docs/KNOWN_LIMITATIONS.md` for what V1 does not do.
