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
- Pilot each one (right-click → Pilot) and frame the shot. Keep the four views close enough that the Google tiles don't reload on every move.
- Labels are any actors (a `TextRender` is enough) with actor tags **`LFF_Label`** plus one of `LBL_Plaza`, `LBL_Gate`, `LBL_Concourse`, `LBL_Restroom`, `LBL_Food`, `LBL_Section`. They're hidden except on the steps that list them.
- Every label and proxy says **"Illustration"** or "Example". None of them names a real gate, restroom or sensory room.

## 4. Import the journey table
Right-click in `/Game/LFF/Data` → **Import** → `SourceData/Journey/LFF_GoldenPath.json` → Row type **`LFFJourneyStep`** → save as **`DT_LFF_GoldenPath`**. To update it later, edit the JSON and then choose Reimport.

## 5. Create `/Game/LFF/UI/WBP_LFF_HUD`
**User Interface → Widget Blueprint → parent `LFFHUDWidget`.** Use the names in `Docs/JOURNEY_SCHEMA.md` → "WBP_LFF_HUD widget names". Suggested layout at 1920×1080:
- **Top bar:** `PrototypeBadgeText` on the left. `PauseButton` (`PauseLabel`), `SoundButton` (`SoundLabel`) and `SettingsButton` on the right.
- **Top centre:** `DisclaimerPanel` holding `DisclaimerText`. Use a neutral colour, at least 28 pt, with no animation.
- **Left card:** `ProgressText`, `TitleText` (48 pt) and `BodyText` (32 pt), plus `TargetMissingText`.
- **Bottom:** `BackButton` on the left and `NextButton` (`NextLabel`) on the right. Keep them at least 400 px apart and put the key hints in the button text. Put `RestartButton` ("Start again? (R)") next to Back. Keep the bottom 60 px clear for the Cesium/Google credits.
- **Right-side panel `TogglesPanel`.** C++ shows it on the intro and while Settings is open. From top to bottom:
  - `IntroPanel`, a "Before you start" header.
  - `SettingsPanel`, a "Settings" title bar with `SettingsCloseButton`.
  - `ReducedMotionButton`, `CalmModeButton` and `LargeTextButton`, each with its `*Label`.

  Widget names must be unique, so the toggles exist once and the two headers share them. `SoundButton` stays in the top bar so it's always reachable.
- **Overlays** (collapsed by default; C++ shows them):
  - `PausePanel`, containing `ResumeButton`.
  - `RestartConfirmPanel`, containing `RestartYesButton` and `RestartNoButton`.
- **Every button:** turn off **Is Focusable** (C++ also hands focus back to the viewport after each click).
- **Event Graph (optional):** handle **On Snapshot Changed** for the Calm Mode muted palette and for large text (render scale about 1.25 on the card). Don't hide the disclaimer or the badge; C++ shows them again anyway.

## 6. Verify in PIE
Run the PIE checklist in `Docs/TEST_PLAN.md`.
