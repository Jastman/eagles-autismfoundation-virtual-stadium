# Cesium placement: reproducible editor setup (LFF)

This implements ADR-0001. Targets UE 5.8 and Cesium for Unreal v2.28.0+.
- **UNKNOWN** means the item isn't confirmed. Check it on the named page before relying on it.
- **[VER]** means the item depends on the plugin version. Re-check it after any Cesium upgrade.
- API reference pages live at `https://cesium.com/learn/cesium-unreal/ref-doc/<page>`.

## 1. Actor list and hierarchy (map `/Game/Maps/LFF_Prototype`)
```
CesiumGeoreference          Origin Placement = Cartographic Origin; Lat/Lon/Height from DA (§2)
CesiumSunSky
CesiumCreditSystem          auto-spawned; its on-screen credits must stay visible (Google logo/attribution)
Tileset_Context             Cesium3DTileset: Google Photorealistic 3D Tiles via Cesium ion (user decision, LEDGER F29)
  └ ClipOverlay             CesiumPolygonRasterOverlay: Polygons=[CartoPoly_LFF_Footprint], Exclude Selected Tiles=on, Invert Selection=off
Tileset_OSMBuildings        optional; only with terrain stack; own CesiumPolygonRasterOverlay, same polygon
CartoPoly_LFF_Footprint     CesiumCartographicPolygon (spline + own GlobeAnchor), points from DA.ClipPolygon
LFF_SiteRoot                Actor + CesiumGlobeAnchorComponent at DA origin LLH, ESU rotation = identity
  ├ LFF_Model               stadium mesh(es); RELATIVE transform = DA model correction; proxy = SM_LFF_Proxy
  ├ LFF_Targets/JT_*        ALFFCameraTarget (new), one per TargetId, attached, no GlobeAnchor
  ├ LFF_ControlPoints/CP_*  editor-only markers at model features used in §4/§6
  └ LFF_FallbackGround      plane/apron mesh, hidden unless Offline mode (§7)
StadiumTourCamera           plain ACameraActor subclass; NOT DynamicPawn; no CesiumOriginShiftComponent
```
Rules:
- Only `LFF_SiteRoot` in the site hierarchy has a GlobeAnchor.
- Set its `AdjustOrientationForGlobeWhenMoving` to ON, which is the documented default. [VER]
- No actor in the map has `UCesiumOriginShiftComponent`.
- World Settings: leave World Origin Rebasing OFF.
- Leave the georeference `Scale` and `Ellipsoid` at their defaults (100 %, WGS84). UNKNOWN: whether these property names exist in 2.28. Check `classACesiumGeoreference.html`.

## 2. `DA_LFF_GeoPlacement` (class `ULFFGeoPlacementDataAsset : UPrimaryDataAsset`, to be written)
| Field | Type | Notes |
|---|---|---|
| SchemaVersion | int32 | Starts at 1. Bump on any field change. |
| PlacementRevision | FString | e.g. `2026-10-02a`. Also record it in the LEDGER. |
| CalibratedBy / CalibratedOnUtc | FString / FDateTime | Who calibrated it and when. |
| OriginLatitudeDeg / OriginLongitudeDeg | double | WGS84, at least 7 decimals (1e-7° ≈ 1 cm). Midfield centre. |
| OriginHeightEllipsoidM | double | **Ellipsoidal.** This is the value written to OriginHeight. |
| OriginDescription | FString | e.g. "midfield, centre of 50-yd line, turf level" |
| PublishedOrthometricM, GeoidUndulationM, GeoidModel | double, double, FString | Provenance only (e.g. 3.0, -33.x, "GEOID18"). |
| SampledTileSurfaceM, HeightSource | double, FString | Measured in §4 step 3. Decides OriginHeightEllipsoidM. |
| ModelAsset | TSoftObjectPtr<UObject> | Mesh or BP. Also record ModelVendor / ModelVersion / ModelSha256 (FString). |
| ModelSourceUnit | FString | e.g. `cm`, `m`, `in`. Found in §4 step 1. |
| ModelUniformScale | double | Applied to LFF_Model. Non-uniform scale is not allowed. |
| ModelRotationDeg | FRotator | Relative to SiteRoot (Unreal: yaw 0 = +X = East). |
| ModelOffsetCm | FVector | Relative to SiteRoot, in the E/S/Up frame. |
| ClipPolygonLonLat | TArray<FVector2D> | Footprint and buffer, clockwise from above. Height isn't needed. |
| ClipBufferM | double | Default 4.0. |
| ControlPoints | TArray<FLFFControlPoint> | Each has {FName Id; FString Desc; double LatDeg, LonDeg, HeightEllM; FVector ModelLocalCm; double TolM}. |
| MaxGuidedRadiusM | double | Default 1500. Validation fails if any target is beyond it. |
| Residuals | FLFFPlacementResiduals | {HeadingDeg, HorizRmsM, HorizMaxM, VertMaxM, ScaleErrPct}, written by the validator. |
| ContextStack | enum {GooglePhotoreal, WorldTerrainOSM, Offline} | Default for this build. |

What never goes in the DA:
- **Tokens.** See §7.
- **Journey data.** See §8.

How the DA is used:
- An editor `CallInEditor` action, "Apply Placement", writes the DA values to the georeference, SiteRoot, LFF_Model and CartoPoly.
- "Validate Placement" runs §6 and writes the Residuals. It also runs automatically on BeginPlay, but **read-only**.

UNKNOWN: the exact setter and transform function names to call from C++ or Blueprint. Candidates are `SetOriginLongitudeLatitudeHeight` and `TransformLongitudeLatitudeHeightPositionToUnreal`/`TransformUnrealPositionToLongitudeLatitudeHeight` (these were renamed across versions [VER]). Check `classACesiumGeoreference.html` and `classUCesiumGlobeAnchorComponent.html` (`MoveToLongitudeLatitudeHeight`, `SetEastSouthUpRotation`).

## 3. Seed values (VERIFY, never final)
The address is 1020 Pattison Ave. Seed coordinates are 39.9005–39.9015 N, -75.1672 to -75.1681 W. Published sources disagree by about 42 m.

Expected ENU offsets from a seed of 39.9008, -75.1675. They're good only to about ±50 m because the landmark coordinates are coarse:

| Landmark (source coords) | E m | N m | Dist m | Bearing |
|---|---|---|---|---|
| Citizens Bank Park (39.90583, -75.16639) | +95 | +558 | 567 | ~10° |
| Xfinity Mobile Arena, ex-Wells Fargo Center (39.90111, -75.17194) | -380 | +34 | 381 | ~275° |
| NRG Station, Broad St (39.9054, -75.1732) | -487 | +511 | 706 | ~316° |

Pattison Ave and Broad St: identify them in the imagery. We don't claim their geometry here.

Height: if N ≈ -33 m (VERIFY with the NGS GEOID18 tool at the final lat/lon), ground is about 3 + (-33) ≈ **-30 m ellipsoidal**. The tile sample in §4 step 3 is authoritative. Never enter 3.

Datum note: WGS84 and NAD83(2011) differ by about 1–2 m here. Record which datum each coordinate uses. Tiles are WGS84.

## 4. Calibration procedure (engine workstation, Cesium tiles online)
0. **Seed the origin.** Set the seed LLH with height -30. Load `Tileset_Context`. Clipping stays off during calibration.
1. **Scale and units.** In the model, measure the field of play from end line to end line. NFL rule: 120 yd = 109.73 m. The goal line to goal line check is 100 yd = 91.44 m.
   - `ModelUniformScale = 10973 cm / measured length`, in native import units.
   - Cross-check the goal-line distance. Both must agree within 0.3 %.
   - Record `ModelSourceUnit`.
2. **Pick the origin.** In the tiles, find the midfield centre: the midpoint between the two goal-post bases, or the 50-yard-line hash midpoint.
   - Read its LLH with the Cesium picking or transform function (UNKNOWN name, see §2).
   - Set this as Origin Lat/Lon to at least 7 decimals, then re-centre.
3. **Height.** Line-trace down onto the tile surface at the midfield point and at 3 or more open-ground points (plaza or parking lot). Convert each hit to LLH height. That gives SampledTileSurfaceM.
   - `OriginHeightEllipsoidM` = turf sample.
   - Sanity check: |sample − (PublishedOrthometricM + GeoidUndulationM)| ≤ 3 m. A larger gap means a datum error or a bad sample, so investigate.
   - The field may sit below street grade, so don't use a street sample as the turf height.
4. **Heading.** Place `CP_GoalPostN` and `CP_GoalPostS` (or the end-line midpoints) on the tiles. Their ENU vector gives the field axis azimuth.
   - Measure the same axis in model space.
   - `ModelRotationDeg.Yaw` = the difference. Pitch and roll stay 0 unless the model is tilted.
   - Repeat using the long edges of the stadium roof or bowl as a second estimate. The two must agree within 0.5°.
5. **Offset.** Set `ModelOffsetCm` so the model's midfield lands at SiteRoot (0,0) and its turf lands at Z = 0.
6. **Control points.** Add 5 or more CPs spread around the bowl: 2 goal posts, the NE, NW, SE and SW exterior corners or gate canopies, and 1 roofline point.
   - For each, store the tile LLH plus the matching model-local position.
7. **Coarse landmark check.** Compare the §3 bearings to the tile landmarks. This only catches gross errors (90° or 180° flips, wrong units).
8. **Clip polygon.** Trace the model footprint plus `ClipBufferM` into `ClipPolygonLonLat`. Enable clipping. The model apron must overlap about 1–2 m outside the polygon, raised +5 cm (§6).
9. **Record the results.** Run "Validate Placement", save the DA, and record the PlacementRevision, residuals and screenshots in the LEDGER.

## 5. Precision and z-fighting
- Everything guided stays within 1.5 km of the origin. UE5 LWC doubles make precision a non-issue at that range. **Don't** add origin shifting for "precision".
- Z-fighting comes mainly from coplanar tile and model ground. The fixes:
  - Clip the tiles under the footprint.
  - Raise the model apron +5 cm above the tile surface where they overlap.
  - Never place model geometry exactly on the tile surface.
- Keep the camera near-clip at the engine default. Don't frame shots that graze long coplanar surfaces.
- Tile LOD refinement can "pop" during flights. Pre-warm by flying the golden path once in the preflight (§7). Tuning `MaximumScreenSpaceError` is a later perf task. [VER]

## 6. Alignment validation checklist (G3). Tolerances are proposed; the lead may tighten them.
| # | Check | Pass |
|---|---|---|
| V1 | Level georeference Lat/Lon/Height equal the DA values | ≤ 1e-9°, ≤ 1 mm |
| V2 | SiteRoot anchor LLH equals the origin, and its ESU rotation is identity | ≤ 1 cm, ≤ 0.01° |
| V3 | Field length after scaling (end line to end line) | 109.73 m ± 0.3 % |
| V4 | Heading residual: goal-post axis versus tiles | ≤ 0.5° |
| V5 | Control-point horizontal residual (model versus tiles) | RMS ≤ 1.0 m, max ≤ 2.0 m |
| V6 | Vertical: model turf and apron versus tile ground at control points | ≤ 0.5 m. No visible floating or burying at gates. |
| V7 | Coarse landmarks (§3) | Bearing within ±8°, distance within ±60 m |
| V8 | Clip: no tile geometry visible inside the bowl or concourse from any target pose | 0 visible, by screenshot per pose |
| V9 | Structure: exactly 1 GlobeAnchor in the SiteRoot tree, 0 OriginShift components, rebasing off | Exact |
| V10 | Every step's TargetId resolves to exactly one JT_ actor under SiteRoot, within MaxGuidedRadiusM | 100 % |
| V11 | Change the origin by +0.001° lat, run Validate, then revert. Every target keeps its pose relative to the model. | ≤ 1 cm |
| V12 | V1–V10 pass again in a **packaged** Development build, with the log attached | Pass |

## 7. ion token, network and offline fallback
- **Token.**
  - Use a dedicated ion token for this demo only. Scope it to the used asset IDs, with `assets:read` only.
  - Revoke it after the demo.
  - It ends up in the packaged build, so treat it as extractable.
  - **Verified (v2.29.1):** the default token is `DefaultIonAccessToken` on `Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset`. That folder is git-ignored (2026-09-28). Choose the token per machine in the Cesium panel → Token.
  - The token must not be committed to git. Before any commit that touches Cesium assets, check the diff and git-ignore any asset that holds the token. UNKNOWN whether a per-tileset token override can be read from a local kiosk ini at runtime. Check `classACesium3DTileset.html`.
- **Preflight at launch.**
  - The kiosk shows a neutral loading screen. It waits for `Tileset_Context` to finish loading at the first 2 target poses, with a timeout of 20 s.
  - The session mode is then **fixed** to Online or Offline and never switches mid-session (predictability).
  - UNKNOWN: the exact load-progress API and the load-failure delegate names. Check `classACesium3DTileset.html`.
  - Operator override: a launch argument or kiosk-ini flag `ForceOffline`.
- **Offline mode.**
  - Tilesets are disabled.
  - `LFF_FallbackGround` and a neutral surround (low proxies of neighbouring venues, labelled) are shown.
  - Credits aren't needed.
  - The journey works identically because it never depends on tiles.
- **Network loss mid-session.** Keep whatever tiles are already resident, with no visual swap. Log it, and have the attendant restart into Offline if needed.
- **Don't treat the Cesium request cache as offline storage.** That's `cesium-request-cache.sqlite`, pruned by `MaxCacheItems`/`RequestsPerCachePrune` in `UCesiumRuntimeSettings`. Google Map Tiles policies prohibit offline use and unauthorized caching.
- **Known issue.** Packaged apps have failed to load tiles. The known workaround is to delete the sqlite cache and flush DNS (Cesium forum #36043). Put it in the kiosk runbook.
- **Local offline tiles.** These would be a `Cesium3DTileset` with Source = From URL, `file:///...`, and would need licensed data that allows offline use. That licence is an open question for the lead.

## 8. Journey-system contract (MUST / MUST NOT)
**Steps MAY store:**
- `FName TargetId` (a `FGameplayTag` is allowed later)
- text, media, duration, DisclaimerLevel, and accessibility flags

**Steps MUST NOT store:**
- world, ECEF or LLH positions
- rotations in world space
- Cesium object references
- georeference values

That rules out `FStadiumScheduleStop::WorldLocation` and `CameraRotation`. Both are removed in the refactor.

**Target actors:**
- `ALFFCameraTarget` holds `TargetId`, a camera-pose child component, and an optional look-at point.
- It's authored in the level **attached under `LFF_SiteRoot`**, so its pose lives in the local stadium frame.
- The camera reads the target's current world transform at transition time, never a cached one.

**Journey code MUST NOT:**
- include Cesium headers
- change or read the georeference origin
- spawn origin-shift components
- assume the origin is (0,0,0)

**Missing targets fail closed.** An unresolved TargetId shows "This step is not available yet" and logs an error. It never flies to (0,0,0).

**Model swap.** Targets stay put. Re-validate V5, V6, V8 and V10, and move any JT_ that now sits inside geometry. Record the move in the LEDGER.

## 9. CesiumSubLevel
Not used in v1: there's one site and one fixed origin. Reconsider it only if we add multiple venues or a city-scale fly-in beyond MaxGuidedRadiusM. If that happens, re-evaluate the origin-shift policy and issue #1706 (sub-level actors at the world origin after `OpenLevel`).

## V1 amendments (2026-09-28, ADR-0003 and LEDGER D21)
- **Context tiles: Google Photorealistic 3D Tiles**, streamed from Cesium ion. The build is online-only; there is no offline fallback for these tiles. The packaged video is the fallback.
- **Until the CGTrader model is ingested, the Google tiles' own stadium is the exterior stand-in.** Leave the clip overlay **disabled**. Turn it on only once the licensed model is placed (calibration step 8).
- **The attribution must stay visible.** Nothing in the HUD may cover the Cesium/Google credits (bottom strip).
- **Placement storage for V1:** the values live in the map (`CesiumGeoreference` origin, `LFF_SiteRoot` anchor) and are copied into the table below. `DA_LFF_GeoPlacement` is deferred.

| Value | Setting | Recorded by / date |
|---|---|---|
| Origin latitude (deg) | 39.9008 (**seed, not calibrated**) | `build_lff_stadium_level.py`, 2026-09-28 |
| Origin longitude (deg) | -75.1675 (**seed, not calibrated**) | `build_lff_stadium_level.py`, 2026-09-28 |
| Origin height (m, ellipsoidal) | -30.0 (**seed**; replace with the §4 step 3 tile sample) | `build_lff_stadium_level.py`, 2026-09-28 |
| LFF_SiteRoot heading (deg) | 0 (**seed**; field axis assumed north–south) | `build_lff_stadium_level.py`, 2026-09-28 |
| Cesium for Unreal version | v2.29.1 | user, 2026-09-28 |
