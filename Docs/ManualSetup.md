# Workstation setup (UE 5.8.3 + Cesium for Unreal v2.29.1)

These are the one-time editor steps the code can't do. They can be done by hand or by Claude Code through Unreal MCP (`Docs/LOCAL_AGENT_SETUP.md`). Asset names and paths must match exactly, because the code loads them by path.

## 0. Prerequisites
- UE 5.8.3, plus Cesium for Unreal v2.29.1 from Fab, installed to the engine.
- Mac: Xcode. Windows: Visual Studio 2022 with the C++ game workload. Check the exact toolchain versions in the UE 5.8 release notes (**VERIFY**).
- A Cesium ion account with access to **Google Photorealistic 3D Tiles**.

## 1. Build the C++ module
Open `VirtualStadium.uproject` and accept the rebuild. Or use the CLI commands in `Docs/LOCAL_AGENT_SETUP.md` §3. Then run the tests in §3 there. They must pass before you go on.

> **Automated (2026-09-28).** §2–§5 are done by the scripts in `Tools/Editor/` (see `Docs/LOCAL_AGENT_SETUP.md` §5). The steps below remain the reference for what they build. **Still manual:**
> - Choose the project ion token in the Cesium panel (per machine, never committed).
> - Calibrate the origin (`CESIUM_PLACEMENT.md` §4). The script uses the seed values.
> - Pilot and reframe the four `CT_` targets once the tiles stream. The script's poses are seeds placed without tiles.
> - Optionally, the `OnSnapshotChanged` graph for the Calm Mode palette and large text.

## 2. Create the map `/Game/LFF/Levels/L_LFF_Stadium`
1. **File → New Level → Empty Level**. Don't use Open World, because World Partition isn't wanted here. Save it as `/Game/LFF/Levels/L_LFF_Stadium`.
2. **World Settings.** GameMode Override stays empty; `GlobalDefaultGameMode` is `LFFGameMode`.
3. **Cesium panel.** Connect to ion and add **Google Photorealistic 3D Tiles**. That adds a `CesiumGeoreference` and a `Cesium3DTileset`. Add a `CesiumSunSky`.
4. Set the georeference origin by following `Docs/CESIUM_PLACEMENT.md`: midfield, turf height, **ellipsoidal** height. Record the final values in the table there.
5. **Don't** add a `DynamicPawn`, and don't add `CesiumOriginShift` anywhere.
6. Add an empty actor named **`LFF_SiteRoot`** with actor tag `LFF_SiteRoot`, and give it a `CesiumGlobeAnchor` (ADR-0001).
7. **Stand-in stadium (D21).** Until the CGTrader model is in, the Google tiles' own stadium is the exterior. Don't clip the tiles yet. For interior steps, add simple neutral proxy meshes under `LFF_SiteRoot`, such as a concourse corridor made of engine cubes and a seat-row block, each with a floating `TextRender` "Illustration".

## 3. Place camera targets and labels (children of `LFF_SiteRoot`)
- Add four **`LFFCameraTarget`** actors. Set their **Target Id** to `CT_ExteriorArrival`, `CT_GateEntry`, `CT_InteriorConcourse` and `CT_SeatFieldView`.
- **Scripted (2026-09-29):** `python3 Tools/Editor/build_lff_standins.py` (editor open, MCP running) adds the other 11 targets the 16-step journey names, plus the stand-in props and their labels. It's safe to rerun: it removes what it placed before (tag `LFF_StandIn`).
- Pilot each one (right-click → Pilot) and frame the shot. Keep the four views close enough that the Google tiles don't reload on every move.
- Labels are any actors (a `TextRender` is enough) with actor tags **`LFF_Label`** plus one of `LBL_Plaza`, `LBL_Gate`, `LBL_Concourse`, `LBL_Restroom`, `LBL_Food`, `LBL_Section`. They're hidden except on the steps that list them.
- Every label and proxy says **"Illustration"** or "Example". None of them names a real gate, restroom or sensory room.

## 4. Import the journey table and the sounds
Right-click in `/Game/LFF/Data` → **Import** → `SourceData/Journey/LFF_GoldenPath.json` → Row type **`LFFJourneyStep`** → save as **`DT_LFF_GoldenPath`**. To update it later, edit the JSON and then choose Reimport.

**Scripted:** make the sources, then import them all with the editor closed:
1. `python3 Tools/Audio/make_lff_audio.py`: the plaza murmur, the cheer bed and the preview sample → `SourceData/Audio/`.
2. `python3 Tools/Audio/make_vo_script.py` → `SourceData/Audio/VO/VO_MANIFEST.md` and one `.txt` per step (add `--placeholders` for test voices). Put recorded `VO_NN_Name.wav` files there with the same names.
3. `python3 Tools/Pipeline/prepare_ticket.py`: ticket frames from `Assets/EaglesMobileTicket.gif` → `Saved/TicketImport/`.
4. `/Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/prepare_eagle.py -- --src "Assets/bald-eagle/source/Bald Eagle 05a.gltf" --out Saved/EagleImport`.
5. `Tools/Editor/run_headless.sh Tools/Editor/import_lff_content.py "LFF content"` reimports the table and imports the sounds (beds loop), the voice lines (`/Game/LFF/Audio/VO`, never loop), the ticket frames, the eagle and the firework glow material.

## 5. Create the HUD widgets in `/Game/LFF/UI`
**Scripted:** with the editor open and Unreal MCP running, run `python3 Tools/Editor/build_wbp_lff_ui.py` (add `--rebuild` to replace existing widget trees, or `--extend` to add only the session 3 widgets to existing ones). It builds `WBP_LFF_Splash`, `WBP_LFF_TopChrome`, `WBP_LFF_StepCaption`, `WBP_LFF_BottomRail` and `WBP_LFF_Settings` (parent `LFFJourneyPanel`), then `WBP_LFF_HUD` (parent `LFFHUDWidget`) holding one of each. The names and layout rules are in `Docs/JOURNEY_SCHEMA.md` → "HUD widgets".

## 6. Verify in PIE
Run the PIE checklist in `Docs/TEST_PLAN.md`.
