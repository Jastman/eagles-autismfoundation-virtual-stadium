# Manual Unreal Editor setup

The repository contains a code-first starter scaffold, but a few steps must still be completed in the Unreal Editor because `.uasset` files are binary and are intentionally not included in this prototype.

## 1. Install Cesium for Unreal

1. Install Unreal Engine 5.4 or newer.
2. Add the **Cesium for Unreal** plugin to your engine or project.
3. Open `/home/runner/work/eagles-autismfoundation-virtual-stadium/eagles-autismfoundation-virtual-stadium/VirtualStadium.uproject`.
4. If prompted, rebuild the `VirtualStadium` module.

## 2. Create the prototype level

1. Create `/Game/Maps/LFF_Prototype`.
2. Add:
   - `CesiumSunSky`
   - `CesiumGeoreference`
   - `Cesium3DTileset` for terrain or context imagery
3. Optional: add an empty actor that marks the approximate real-world anchor for Lincoln Financial Field.

## 3. Create schedule data

1. In the Content Browser, create a **Primary Data Asset** using `StadiumScheduleDataAsset`.
2. Name it `DA_LFF_ScheduleStops`.
3. Copy the placeholder stop order from `/home/runner/work/eagles-autismfoundation-virtual-stadium/eagles-autismfoundation-virtual-stadium/Content/Data/ScheduleStops.template.json`.
4. Populate each stop's title, description, camera transform, fly duration, preview image, and placeholder settings.
5. Reorder the `Stops` array whenever the game-day sequence changes.

## 4. Add the guided tour camera

1. Place a `StadiumTourCamera` actor in the level.
2. Assign:
   - `Schedule Data` → `DA_LFF_ScheduleStops`
   - `Marker Class` → a Blueprint subclass of `StadiumStopMarker` (recommended name: `BP_StadiumStopMarker`)
   - `Tour Widget Class` → `WBP_StadiumTourWidget`
3. Leave `Auto Spawn Markers` enabled to generate placeholder wayfinding markers for every stop.

## 5. Create the accessible UI widget

1. Create a Widget Blueprint based on `StadiumTourWidget`.
2. Add the following controls:
   - Title text
   - Description text
   - Preview image
   - Progress text and/or progress bar
   - Previous, Next, and Restart buttons
3. Name the buttons `PreviousButton`, `NextButton`, and `RestartButton` so the C++ widget binds automatically.
4. Bind text and image display to:
   - `GetCurrentTitle()`
   - `GetCurrentDescription()`
   - `GetProgressText()`
   - `GetCurrentPreviewImage()`
5. Accessibility recommendations:
   - Minimum 18px-equivalent body text
   - High contrast colors
   - Persistent progress indicator
   - Large touch-friendly button targets
   - Plain-language descriptions with one main instruction per paragraph

## 6. Create placeholder markers

1. Create `BP_StadiumStopMarker` from `StadiumStopMarker`.
2. Assign an optional sprite texture to the billboard component for generic wayfinding signs.
3. Use engine starter meshes (cone, cube, plane) or neutral custom meshes for interior areas without detailed geometry.
4. Keep placeholder art generic and avoid copyrighted team branding.

## 7. Verify interaction

In Play-In-Editor:

- `Left Arrow` / `Gamepad D-Pad Left` → previous stop
- `Right Arrow`, `Space`, or `Gamepad D-Pad Right` → next stop
- `R` or `Gamepad Bottom Face Button` → restart the tour
