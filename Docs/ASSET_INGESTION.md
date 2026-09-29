# Stadium Asset Ingestion SOP

**Target:** Lincoln Financial Field model, CGTrader "Eagles Stadium Philadelphia 3D Model", **received 2026-09-29** in `Assets/`, which is git-ignored.

**As delivered (measured):**
- About 7.2 M triangles: stadium plus site 3.47 M, NFL field 40 k, seats about 3.7 M.
- OBJ, FBX, DWG and 3DM. The FBX has no textures.
- 152 textures.

**How it's integrated:** see **ADR-0004**. Where this SOP and ADR-0004 differ, ADR-0004 wins. Pipeline:
1. `Tools/Pipeline/prepare_stadium.py` (Blender)
2. `Tools/Editor/import_stadium.py`
3. `Tools/Editor/setup_cinematic_look.py`

**Corrections to the draft below:**
- **Nanite supports masked materials** (since UE 5.1). Only translucent materials are excluded, and this model has none.
- **The objects are generically named** (`object_N`), so "preserve named objects" doesn't apply. The sections are spatial chunks.
- **Texture budget:** about 1 GB at full resolution, streamed. The limit that matters is the 2 GiB release zip (D15), measured at package time.
**Gating:** Approval at each checkpoint before proceeding. No assets become a build dependency until they pass validation.

## Pre-Import Gate (VERIFY with vendor and legal)

1. **License verification:**
   - Confirm license text covers UE 5.8 use, Win64 redistribution, and public/kiosk display.
   - VERIFY: Can the asset be packaged in a closed-source commercial demo? (Vendor policy, not legal advice.)
   - Confirm modification rights (textures, materials, colliders, visibility groups).
   - Store the signed license agreement in `/Project/Vendor/LFF_License.pdf` (outside git).

2. **Vendor identity & delivery:**
   - VERIFY exact vendor: CGTrader link vs 3dmodels.org brief (F11 ambiguity).
   - Confirm download format preference: **FBX recommended for UE import** (Interchange supports both FBX and glTF; assess asset-specific needs: FBX better for skeletal rigs, glTF better for web export, USD for multi-software pipelines).
   - Obtain SHA-256 hash from vendor or compute after download.
   - Download to a durable archive outside the repo and outside `Content/` (e.g. `D:\LFF_SourceArchive\<vendor>_<date>\` plus a backup copy on team storage). Never use a temp directory. Record the SHA-256 in this file.

3. **Asset receipt & integrity:**
   - Compute `sha256sum <archive>` and compare to vendor documentation (or create baseline).
   - Extract once to a staging directory; do **not** commit the archive or extracted files to git.
   - List all files: `find . -type f | sort` and verify against a manifest.

## Import Workflow (UE 5.8 + Interchange / FBX)

### Step 1: Scale, axes, pivot verification (VERIFY before proceeding)
- In Unreal Editor, create a test level `/Game/LFF/Temp/ModelReview`.
- Drag the source FBX / glTF into the viewport.
- **VERIFY scale:** Expected ~400 cm height for a stadium structure. If vastly off, note the scaling factor.
- **VERIFY axes:** UE uses Z-up; most models are Y-up. Check Interchange import settings: `bConvertScene` and `bImportAsSkeletalMesh`.
- **VERIFY pivot:** Is the model origin at the stadium center or at one corner? Document and confirm with design team.
- Screenshot the test model and log findings in a checklist.

### Step 2: Texture & material baseline
- Count textures by format (PNG, TGA, EXR, others) and resolution distribution.
- Estimate total uncompressed size: `du -sh <texture_folder>`.
- VERIFY: Are textures embedded or external? Embedded textures require extraction in Blender or Substance 3D Designer.
- Note any missing or corrupt texture references; do not proceed if critical textures are absent.

### Step 3: Structured import to `/Game/LFF/Assets/Stadium/Imported`
- Create folder hierarchy:
  ```
  /Game/LFF/Assets/Stadium/
    Imported/              (untouched import; no manual edits)
      LFF_Model_v1        (or vendor-supplied name)
      Materials/
      Textures/
    Optimized/            (modified version for runtime)
      Sections/
        Bowl/
        Exterior/
        Field/
        Roof/
        VideoBoards/
      Materials_Optimized/
    Placeholders/
      ProxyStadium/       (labeled stand-in)
  ```
- Use Interchange or native FBX import:
  - Import meshes as static meshes (not skeletal, unless rigged).
  - Let materials import; review for transparency, two-sided flags.
  - Preserve named objects (critical for visibility toggling).

### Step 4: Topology & transparency review
- Open each imported mesh in the Static Mesh Editor.
- **Nanite suitability per part:**
  - Opaque surfaces (bowl, seats, walls): candidate for Nanite (reduces draw calls, improves performance).
  - Masked/translucent surfaces (windows, signage): may require per-material decisions; Nanite doesn't support masked, so rasterize those.
  - Note any two-sided surfaces (windows from inside/outside); enable `TwoSided` in material.
- Log which sections use Nanite and which stay rasterized.

### Step 5: Collision & camera interaction
- Delete auto-generated collision if present (expensive and unnecessary for a camera-only tour).
- For any touch-point (e.g., interactive marker zones), use simple box colliders; do not use per-poly collision.
- Confirm `Collision Enabled` = `Query Only` (no physics simulation needed).

### Step 6: Sectioning & visibility groups
- In the Optimized folder, create separate static meshes or instanced collections for:
  - **Bowl** (seating bowl, upper deck).
  - **Exterior shell** (roof overhang, outer walls).
  - **Field** (playing surface, markings if baked).
  - **Roof/canopy** (structural elements above seats).
  - **Video boards** (scoreboard, ribbon boards—potential animation candidates).
- Update the Data Asset (`DA_LFF_ScheduleStops`) to reference these sections; allow toggling visibility by stop.

### Step 7: Material consolidation & texture budget
- Count unique materials across the asset.
- Identify opportunities to merge similar materials (same texture + color) to reduce draw calls.
- Measure combined texture memory:
  - List all textures and their resolutions (e.g., 4K, 2K, 1K).
  - Target: stay under 1 GB total for packaged build (including all LODs).
- If textures are excessive, downscale lower-priority areas (roof, exterior details) to 1K.

### Step 8: Reimport strategy documentation
- Create a **Reimport Checklist** for future asset updates:
  - Vendor SHA-256 of new version.
  - Changelog (geometry changes, texture updates, new features).
  - Test level for regression (compare draw calls, memory, performance vs. previous baseline).
  - Approval sign-off before replacing Optimized versions in production.

## Performance Baseline & Measurement Plan

### Capture (before shipping)
- Use **Stat detailed** in the editor console to record:
  - `StatUnit`: Frame time, game thread, render thread, GPU.
  - `Stat RHI`: Draw calls, primitives, texture memory.
  - `Stat Nanite`: Cluster counts, memory, pixel rates (if Nanite is enabled).
  - `Stat Lumen` (if global illumination is used): GI update cost.
- Create a test level `/Game/LFF/Perf/BaselineLevel` with one complete section (e.g., bowl + exterior).
- Stand the camera at the closest and farthest expected viewpoints; capture stats.
- Package a Win64 test build and record the same stats on the target hardware (if available).
- Log the exact commands used (candidates: `stat unit`, `stat gpu`; VERIFY each in UE 5.8 before relying on it). Store the results in `Docs/PERF_BASELINE.md`.

### Regression checks (during development)
- Before each major feature merge, re-run the baseline level and compare:
  - Target: PROVISIONAL, lead-owned, to be set once the target hardware is known. Smooth camera motion matters for comfort, so start with a stable 60 FPS at the demo resolution, with 30 FPS as the floor for the reduced-motion fallback. This is not a standard; confirm it on hardware.
  - Alert if draw calls increase >20% or GPU memory rises >50 MB.

## Labeled Proxy Stadium (Placeholder)

Until the real asset is validated and integrated:

**Specifications:**
- Simple bowl extruded from a 2D footprint traced from Cesium imagery (measure it; don't hard-code dimensions from memory).
- Green-and-white placeholder material with large text: **"Illustrative Stand-In — Production Model Pending"**.
- Divided into named sections (Bowl, Exterior, Field, Roof, Boards) for consistency with the final import structure.
- Collision: Query only, minimal cost.
- File location: `/Game/LFF/Assets/Placeholders/ProxyStadium/BP_ProxyStadium.uasset`.
- Used in `DA_LFF_ScheduleStops` until the optimized imported model is approved and swapped.

## Sign-Off Checklist

- [ ] Vendor license signed and stored.
- [ ] Asset SHA-256 verified.
- [ ] Scale, axes, pivot confirmed.
- [ ] Texture inventory and size baseline recorded.
- [ ] Materials and transparency reviewed.
- [ ] Nanite suitability documented per section.
- [ ] Sectioning complete (Bowl, Exterior, Field, Roof, Boards).
- [ ] Draw call & texture memory under budget.
- [ ] Reimport strategy documented.
- [ ] Performance baseline captured on target hardware.
- [ ] Design/product approval of sectioned visibility and placeholder swap points.
- [ ] Legal approval of license and redistribution terms.

---

**Status:** IN PROGRESS (2026-09-29).
- **Asset:** received and inventoried.
- **Scale, axes and pivot:** confirmed (metres, Y-up, midfield origin).
- **Materials and transparency:** reviewed (no transparency, 20 alpha PNGs, masked materials).
- **Nanite:** used for all chunks.
- **Performance baseline and license:** open.
