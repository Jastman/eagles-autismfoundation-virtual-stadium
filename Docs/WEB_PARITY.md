# Web build parity ledger

The web build (`web/`, CesiumJS, iPad Safari first) and the Unreal build are **one experience**. This file is where the
two stay in step: every way the web build differs from Unreal is a row here, and every feature or fix made on the web
is a row here until Unreal has it too.

Run `cd web && npm run check-parity` before committing either side. It reports both directions and exits 1 when
anything is outstanding.

## How the two builds share content

| Content | Source of truth | How the web gets it | Drift possible? |
|---|---|---|---|
| Journey steps, copy, timings, cues, amenities | `SourceData/Journey/*.json` | Imported directly at build time (`web/src/journey/data.ts`) | No |
| Audio: VO, ambience, anthem, UI sounds | `SourceData/Audio/**.wav` | Transcoded to AAC + MP3 by `web/tools/prepare_assets.mjs` | No |
| Karaoke word timings | `SourceData/Audio/VO/*.words.json` | Copied by `prepare_assets.mjs` | No |
| UI images, logos, project font | `SourceData/UI`, `SourceData/People` | Copied by `prepare_assets.mjs` | No |
| Crowd seats, treads, queue, circulation | `Content/LFF/Crowd/*.json` | Copied by `prepare_assets.mjs` | No |
| Level placement: camera targets, paths, cues, labels, places, stand-ins | `Content/LFF/Levels/L_LFF_Stadium.umap` | `python3 web/tools/extract_umap.py && node web/tools/build_scene_data.mjs` → `web/data/scene.json` | **Yes**: rerun after saving the level. `check-parity` flags it. |
| Behaviour: state machine, journey subsystem, camera rig, HUD, input, cues | `Source/VirtualStadium/**`, `Tools/Editor/build_wbp_lff_*.py` | Ported by hand to TypeScript | **Yes**: `check-parity` flags each changed file and names its web counterpart (`web/parity/sources.json`). |
| Stadium, seats, NFL field | CGTrader model (licensed) | Cesium ion assets 6040628, 6040635, 6040637 (team account, scoped token in `web/.env.local`) | Placement only; see W12 |

## Rules

1. **A web change to behaviour, copy, timing or UX goes in the table below** with Unreal status `Pending`, in the same
   commit. Copy and timing changes belong in `SourceData/Journey/*.json` so both builds get them for free; prefer that
   over a web-only change.
2. **An Unreal change to a hand-ported file** is ported to the web counterparts listed by `check-parity`, then accepted:
   `node tools/check_parity.mjs --accept <file>`.
3. **After saving `L_LFF_Stadium.umap`**, rerun the extract (table above) and accept the umap.
4. When Unreal takes a `Pending` row, set it to `Done (<date>, <commit>)`. Rows that only make sense on one platform
   are `N/A` with the reason.

Unreal status values: `Pending` (Unreal must take this), `Done (...)`, `N/A (reason)`.

## Web differences and additions

| ID | Change | Kind | Web files | Unreal status | Notes |
|---|---|---|---|---|---|
| W1 | Touch flight controls in Explore: a move stick and Up / Down buttons, bottom left | Platform adaptation | `ui/hud.ts`, `input/input.ts` | N/A (Unreal targets keyboard, mouse and controller) | Mirrors the WASD / E / Q bindings at the same speeds |
| W2 | Touch wording: Explore help "Drag look · Pinch closer · Pad move · Up / Down"; Settings footer "Tap a setting to change it. Close returns to the stadium." | Platform adaptation | `ui/hud.ts` | N/A (shown only on touch screens; desktop shows the Unreal wording) | Keycap hints stay visible as in Unreal |
| W3 | Drag-to-look is inverted on touch (content follows the finger), pinch dollies like the mouse wheel | Platform adaptation | `input/input.ts` | N/A | |
| W4 | Crowd is billboard fan cards only (39,680 at 70% occupancy). The detailed animated neighbours near My Seat are not drawn | Performance reduction | `scene/crowd.ts` | N/A | iPad GPU budget. The same seats, occupancy and visitor clearing are used |
| W5 | City Sample cars, MetaHumans and the eagle are simple shaded stand-ins at their level positions | Licensing stand-in | `scene/props.ts`, `scene/eagle.ts` | N/A (Unreal keeps the licensed assets) | The eagle and props prefer a `.glb` from `web/assets-src/models/` when one exists |
| W6 | Ticket animation uses the 128 × 256 editor thumbnails until full-resolution frames are exported to `web/assets-src/ticket/` | Asset gap | `tools/prepare_assets.mjs` | N/A | Web-side gap: export the 30 frames from `Assets/` |
| W7 | Audio ships as AAC (iPad, Safari) and MP3 (other browsers), chosen per browser; Unreal plays the WAV sources | Platform adaptation | `tools/prepare_assets.mjs`, `audio/audio.ts` | N/A | |
| W8 | Audio starts on the first touch or key press (iOS requires a gesture) | Platform adaptation | `audio/audio.ts` | N/A | The welcome screen is silent until then |
| W9 | A sound that fails to load is treated as missing for the rest of the session (never retried) | Robustness | `audio/audio.ts`, `journey/journey.ts` | N/A (Unreal sounds are cooked into the package) | Same visitor-facing rule as Unreal: a missing line is skipped and the card stays |
| W10 | If the Google tiles can't stream, a plain ground disc is drawn at turf level | Robustness | `scene/tiles.ts` | N/A | Unreal packages run with a working ion token |
| W11 | Home-screen install (web app manifest, 180 / 512 px icons, full screen, status bar) | Platform addition | `index.html`, `static/` | N/A | |
| W12 | Stadium comes from Cesium ion (3 assets). If ion's placement is more than 2 km from midfield the ADR-0004 registration is applied; `?stadiumHeading=` and `?stadiumOffset=e,n,u` tune it | Platform adaptation | `scene/tiles.ts`, `config.ts` | N/A | Unreal imports the FBX directly. Record any tuning that is needed in ADR-0004 and back in `REGISTRATION` |
| W13 | Deployment settings come from `web/.env.local` or URL parameters (`?ionToken=`, `?stadiumAssets=`, `?stadiumPlacement=`, `?noLoadingScreen`, `?noCrowd`) | Platform adaptation | `config.ts`, `main.ts` | N/A | Tokens are never committed (CESIUM_PLACEMENT §7) |
| W14 | Text and widgets are HTML/CSS with the same layout rules (ShortestSide DPI scaling, 1080 = 1.0, safe bounds) and the Atkinson Hyperlegible font; anti-aliasing and line breaks can differ by a few pixels | Platform adaptation | `ui/hud.css`, `ui/layout.ts` | N/A | |
| W15 | Post-process: the level highlight and the Lights preview exposure lift are rebuilt as CesiumJS post-process stages from `LFF_LevelHighlight.hlsl`; there is no auto-exposure, bloom or cinematic grade | Platform adaptation | `scene/effects.ts` | N/A | |

## Known gaps (web must catch up)

| ID | Gap | Plan |
|---|---|---|
| G1 | Full-resolution ticket frames | Export from `Assets/` into `web/assets-src/ticket/` (git-ignored) |
| G2 | Eagle model | Export the eagle as `web/assets-src/models/eagle.glb` |
| G3 | Stadium placement checked in headless Chromium only (2026-10-15): the ion assets arrive ungeoreferenced, the ADR-0004 registration places them, and the level's lot paint, parked cars, gates and signs line up with the model. Not yet compared side by side with Unreal on an iPad | Compare a few steps on the iPad against the Unreal build; tune W12 if needed |
