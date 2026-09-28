# ADR-0001: Geospatial placement of the stadium, Cesium context and journey targets

**Status:** Proposed  **Date:** 2026-09-28  **Scope:** UE 5.8 with Cesium for Unreal v2.28.0+. No API here has been checked against an installed plugin. Items marked UNKNOWN must be checked on the named doc page before implementation.

## Context
- Steps store raw Unreal `WorldLocation`/`CameraRotation` (`Source/VirtualStadium/Public/Data/StadiumScheduleStop.h`; LEDGER F5, R4). If the georeference origin changes, every step silently lands in the wrong place.
- The purchased model has arbitrary scale, orientation and origin, and isn't in hand yet. A labeled proxy stands in. We can't assume it's georeferenced.
- The kiosk is a packaged Win64 build on one display. Network and ion token status are unknown (LEDGER R6). The audience is autistic fans, so the view must be predictable: no sudden visual swaps mid-session.
- Published coordinates for the stadium don't agree. Wikipedia-derived 39.9008, -75.1675 and latitude.to 39.9005, -75.1672 are about 42 m apart. They're seeds only.
- Cesium heights are WGS84 **ellipsoidal**. The published stadium elevation (about 3 m) is orthometric (roughly NAVD88/MSL). The geoid undulation near Philadelphia is about -33 m (unverified: the NGS service example at 40 N, 80 W gives -33.185 m). So ground is about -30 m ellipsoidal. That figure must be verified, never typed in as 3.

## Decision
1. **One fixed origin.** `CesiumGeoreference` uses Origin Placement = *Cartographic Origin*. OriginLatitude/OriginLongitude/OriginHeight are set to a surveyed point at the field centre (midfield), at turf level, with height in metres above the WGS84 ellipsoid. We store LLH, not ECEF: LLH is what the Details panel edits, people can review it, and ECEF is derived. At the origin, Unreal +X = East, +Y = South, +Z = Up (Cesium "Placing Objects" doc).
2. **One anchored parent.** `LFF_SiteRoot` is the only actor in the site hierarchy with a `CesiumGlobeAnchorComponent`. The stadium model, all journey targets, control points and the fallback ground attach beneath it. The model's scale, rotation and offset correction goes on the model child, never on the anchor. No child carries its own anchor, which would double-apply the transform.
3. **No origin shifting.** Guided mode has no origin shifting of any kind. The tour camera is a plain `ACameraActor`, not `DynamicPawn` (which ships with `UCesiumOriginShiftComponent`). No actor in the map has a `UCesiumOriginShiftComponent`. UE World Origin Rebasing stays off. The guided radius is 1.5 km or less, well within UE5 double-precision (LWC) comfort.
4. **Clip the tiles.** A `CesiumCartographicPolygon` traces the stadium footprint plus a buffer. It feeds a `CesiumPolygonRasterOverlay` on every context tileset, with *Exclude Selected Tiles* on. The model supplies its own ground apron to cover the hole.
5. **One config asset.** `DA_LFF_GeoPlacement` is a new `UPrimaryDataAsset` subclass with a versioned schema, and it is the single source of truth. An editor action applies it to the level. At runtime we only **verify** it (log and on-screen dev error if it doesn't match) and never re-apply it, so the origin can't change at runtime.
6. **Journey contract.** Steps reference target actors by `FName TargetId` (GameplayTag allowed later). Journey code never stores or computes world, ECEF or LLH positions, and never calls Cesium APIs.
7. **Offline policy.** Online or offline is decided once, at launch, by a preflight check. It never flips mid-session. Offline mode means model plus fallback ground plus neutral context. Google Photorealistic 3D Tiles are never cached for offline use.

## Alternatives
| Option | Why rejected (for v1) |
|---|---|
| True Origin / origin at the planet centre | Local coordinates would be around 6,000 km. Worse for authoring, gizmos and float-sensitive systems. Meant for non-georeferenced tilesets. |
| Store the origin as ECEF | Nobody can review it, it isn't the Details-panel input, and it's easy to mistype. It may appear as a derived check value only. |
| GlobeAnchor on the stadium mesh itself | The model correction (scale/yaw) would get mixed with the geo pose, and targets wouldn't inherit it. |
| No GlobeAnchor at all | Fine while the origin never changes. But one accidental origin edit would leave the model and targets floating off the globe. The anchor is cheap insurance. |
| `CesiumOriginShift` in *Change Cesium Georeference* mode | The origin (and all unanchored Unreal coordinates) would rebase whenever the camera moves more than Distance. That breaks stored poses and adds nothing at 1.5 km. |
| CesiumSubLevel (LevelInstance + `UCesiumSubLevelComponent`) | Built for multi-site worlds. Activating one rewrites the georeference origin. Known issue #1706: sub-level actors sit at 0,0,0 after `OpenLevel`. Reconsider only for multi-venue or city fly-ins. |
| Hide whole tilesets / material dither blend | Hiding loses the context. Blending needs custom tileset materials and may still z-fight. Possible v2 polish. |

## Consequences
- The `FStadiumScheduleStop` refactor is required: drop `WorldLocation`/`CameraRotation` and add `TargetId`. `ScheduleStops.template.json` changes to match. `AStadiumTourCamera` resolves poses from target actors.
- A new C++ type is needed for the Data Asset, plus a new `AStadiumJourneyTarget` actor and an editor Apply/Validate utility. `Build.cs` gains a `CesiumRuntime` dependency, but only in the placement module or utility, never in journey code.
- Replacing the model means re-running only the model-correction calibration. Targets are re-validated, not re-authored, unless the geometry changed.
- The ion token ships inside the packaged build, so treat it as semi-public: scoped, dedicated and revocable.

## Validation
Gate G3 passes when every check in `Docs/CESIUM_PLACEMENT.md` §6 passes on the engine workstation, with PIE screenshots and a validation log from a **packaged** build as evidence (LEDGER D5). This ADR moves to Accepted after the first proxy calibration passes and again after the purchased model passes.

Sources (snippets via WebSearch, pages not opened): cesium.com/learn/unreal/unreal-placing-objects/, /unreal-clipping/, /unreal-sublevels/, cesium.com/learn/cesium-unreal/ref-doc/classACesiumGeoreference.html, classUCesiumOriginShiftComponent.html, classUCesiumGlobeAnchorComponent.html, github.com/CesiumGS/cesium-unreal/issues/1706, ngs.noaa.gov/web_services/geoid.shtml, developers.google.com/maps/documentation/tile/policies.
