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

## What is included

- A UE 5.8.3 C++ module (`Source/VirtualStadium`) with Guided Tour and Free Explore, accessibility settings, responsive standard/ultrawide UI, and mouse-drag camera controls.
- Twenty-three illustrative steps in `SourceData/Journey/LFF_GoldenPath.json`, plus the saved stadium map, UI, narration, fitted MetaHuman hair and clothing masks, crowd circulation, sensory-room refinements, and staff greeting animation.
- Cesium request retries, off-screen tile preloading, and a branded startup loading screen.
- Headless automation tests: `LFF.*`.
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

1. Install Git LFS, clone the repository, and run `git lfs pull` before opening Unreal. The stadium, CitySample vehicles, and fitted hair assets are LFS-managed.
2. Install the engine's Cesium and required MetaHuman plugins, including **MetaHuman Creator Core Data** for the fitted hair assets. Optional MCP/editor tooling is disabled in the shared descriptor.
3. Select and save a restricted Cesium ion token on each workstation; `Content/CesiumSettings/CesiumIonServers/` is intentionally ignored.
4. Build and run the tests (`Docs/LOCAL_AGENT_SETUP.md` §3). On Windows, follow [WINDOWS_BUILD.md](WINDOWS_BUILD.md) and use `BuildWindows.cmd`.
5. Open the saved `L_LFF_Stadium` map. `Docs/ManualSetup.md` and editor generation scripts describe earlier setup; do not regenerate or overwrite the current scene as part of normal setup.

## Synchronizing Windows and macOS

The shared project includes the latest saved Windows work from October 8-9,
2026. Keep sources, content, `Config/Default*.ini`, and the runtime plugin
descriptor shared. The Windows renderer/audio section applies only to Windows;
authored `Build/Mac/Resources` files remain available for native Mac packaging.
Do not copy Windows Direct3D settings into shared renderer settings or Mac
configuration. Engine and plugin installations are separate on each workstation.

Before switching computers, save assets, close Unreal, review `git status`, commit
the intended source/assets, and push. On the other computer, with a clean working
tree, run:

```shell
git fetch origin
git pull --ff-only
git lfs pull
```

If that checkout has uncommitted edits or diverged commits, preserve them and
reconcile them before updating; do not force-push or overwrite the newer saved
binary assets. Prefer one active editing workstation at a time for `.umap` and
`.uasset` files, which cannot be text-merged.

`.gitattributes` stores text as LF and checks Windows batch scripts out as CRLF.
Git ignores OS/editor preferences, `Saved`, binaries, caches, packaged Windows
builds, ZIPs, inspection artifacts, and generated packaging records. Keep ion
credentials and Android File Server workstation tokens out of shared config.
The unused Android File Server plugin is explicitly disabled to prevent Unreal
from regenerating a different token in `DefaultEngine.ini` on each computer.
MCP, Terminal, editor toolsets, and VibeUE stay disabled in the committed project;
local enablement must not be included in a synchronization commit.

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

The latest Development package was produced on Windows on October 9, 2026;
see [WINDOWS_BUILD.md](WINDOWS_BUILD.md) for its behavior and rebuild requirements.
The updated project still needs a native Mac build on the Mac workstation.
Earlier prototype gates and decisions remain in `Docs/LEDGER.md` and
`Docs/KNOWN_LIMITATIONS.md`.
