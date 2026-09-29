# ADR-0004: Integrating the official (CGTrader) stadium model

- **Status:** Accepted (lead, 2026-09-29). Measured: 60 fps at 1080p on the M4 Max (Docs/PERF_BASELINE.md). The package size is not measured yet.
- **Date:** 2026-09-29
- **Amends:** ADR-0001 (clipping turns on; placement now comes from model-to-tile registration), D17 (the G2 waiver ends), D21 (the Google stadium is no longer the stand-in).

## Context
The licensed "Eagles Stadium Philadelphia 3D Model" arrived in `Assets/` at 5.2 GB. It comes in four formats:

| Format | Size | Notes |
|---|---|---|
| OBJ + MTL | 2.2 GB | Includes 152 textures |
| FBX 7.7 binary | 1.5 GB | **No embedded media and no texture paths** |
| DWG | 264 MB | CAD, not used |
| 3DM | 1.2 GB | CAD, not used |

Contents, measured with headless Blender:

| Part | Triangles | Details |
|---|---|---|
| Stadium plus site | 3.47 M | 88 materials. It includes 40k generically named objects and a paved site slab of about 480 × 460 m. |
| "Football Field NFL" | 40 k | Includes goal posts |
| "Soccer Field" | not used | Alternative playing surface |
| 3D seats | about 3.7 M | Five parts |

Units are metres, Y-up. The origin sits at midfield turf.

Textures: 36 are about 4K, 49 are 2K, and the rest are smaller. Twenty PNGs have alpha (grills, meshes, banners). The only material data is base colour and normal maps.

Separately, the project was rendering with UE4-era settings: no global illumination, screen-space reflections only, no virtual shadow maps, no distance fields. The hand-written `DefaultEngine.ini` never set the UE5 defaults.

## Decision
1. **Source format: OBJ.** It's the only format whose materials resolve to the delivered textures. Use the **NFL field** (user direction); the soccer field isn't imported.
2. **An offline prep step in Blender** (`Tools/Pipeline/prepare_stadium.py`). The output goes to `Saved/StadiumImport/`, which is never committed.
   - **Chunking.** Classify every object as FIELD, SEATS, GROUND or STRUCTURE, then merge into about 30 chunks: 45° sectors × inner/outer ring.
     - Why not one mesh: Lumen distance fields and surface cache degrade on one giant mesh.
     - Why not 40k meshes: they cost 40k draw calls and actors.
     - Chunks also give Nanite streaming and culling sensible granularity.
   - **Registration to the Google tiles.** Match a 5 m height grid of the tiles (line traces) against the model's own heights, searching rotation and offset.
     - Result: 182.25°, (−2.25 m E, +2.5 m N).
     - The median residual is 0.61 m.
     - On elevated stand and roof samples, 65% fall within 2 m, against 48% for the model flipped 180°.
     - The fit is baked into the geometry, so `LFF_Model` sits at identity under `LFF_SiteRoot`. The model's turf defines the site turf at Z = 0.
3. **Unreal import** (`Tools/Editor/import_stadium.py`).
   - Every chunk is a **Nanite** static mesh. Source normals are kept. There's **no collision** (camera-only tour).
   - Meshes get **32 Lumen cards** and **two-sided distance fields** for the thin roof and canopy sheets.
   - **Materials:** two two-sided master materials (opaque, and masked for alpha PNGs), plus one material instance per source material. Roughness, metallic and specular are set by material category, because the source has no PBR maps.
     - **Glass stays opaque and glossy**, so it keeps Nanite; that reads correctly at stadium scale.
     - **LED and display materials glow statically.** Nothing animates or flashes (ACCESSIBILITY.md).
4. **The Google tiles are clipped over the model site** with a `CesiumCartographicPolygon` and a `CesiumPolygonRasterOverlay` (Exclude Selected Tiles). The model supplies the stadium, plaza and lots; the tiles supply the city beyond.
   - The polygon is the site slab outline inset 1 m, so the model ground overlaps the tiles at the seam.
   - The model plaza is at +1.4 to +1.6 m and the tile ground at +0.7 to +1.2 m, so the seam step is under 1 m.
5. **The proxy concourse is removed.** The model has real concourses, and the old proxy box collided with the photogrammetry.
   - `CT_InteriorConcourse` now stands in the model's open-air perimeter concourse at plaza level. Probed in the model: floor +1.58 m, about 6 m clear height, inner wall at u ≈ −72.3 m.
   - The other three targets were checked against the model and kept.
6. **Cinematic look.**
   - **Project settings** (`DefaultEngine.ini`): Lumen GI and reflections, virtual shadow maps, mesh distance fields, no static lighting, TSR.
   - **Level** (`Tools/Editor/setup_cinematic_look.py`): an unbound post-process volume with high-quality Lumen and gentle histogram exposure (EV 7–15), subtle aerial fog, and the sun at 3:30 pm on Oct 9.
   - **Kept off for comfort:** motion blur, lens flares, grain, chromatic aberration and depth of field.

## Findings during integration
- **The editor stops streaming tiles in the background.** Cesium only gets cameras from visible, real-time, perspective editor viewports. UE's "Use Less CPU when in Background" suspends real-time, so tiles silently stop loading (progress 0, no errors) whenever the editor isn't in front.
  - For automation, `bThrottleCPUWhenNotForeground=False` is set per user in `EditorPerProjectUserSettings.ini`.
- **Lumen cached lighting clipped.** `r.EyeAdaptation.CachedLightingPreExposure` needed 8, not the default 4. At 4, Lumen's cached lighting clipped under the physical sun (the "Cached lighting … is going to be clipped" warning), which crushed the shaded stands to black.
- **The turf texture is too bright** for a physical sun (average sRGB 62/121/26). `MI_LFF_Grass` gets a tint of (0.55, 0.6, 0.5) and specular 0.25. The painted-line materials are separate and unchanged.
- **Headless editor runs don't exit.** In 5.8.3 they hang or crash in `quit_editor` after saving. `Tools/Editor/run_headless.sh` waits for the script's DONE/FAILED line and then stops that exact process.

## Consequences
- **Build size.** About 7.2 M triangles, streamed by Nanite. Textures stream as well. The 2 GiB per-platform release zip (D15) must be verified at package time. The fallback is capping lower-priority textures (roof, exterior back-of-house) at 2K.
- **Quality settings.** The M4 Max runs the high-quality Lumen settings. Weaker demo machines should use scalability "High", not "Epic"/"Cinematic". Record the numbers in `Docs/PERF_BASELINE.md`.
- **License gate (Q-D) still open.** The CGTrader license must allow distribution inside a packaged app before any Release that contains the model.
- **Reproducibility.** Rebuilding means running `prepare_stadium.py`, then `import_stadium.py`, then `setup_cinematic_look.py`. The tile height grid (`tile_dsm.json`) is regenerated only if the georeference changes.
