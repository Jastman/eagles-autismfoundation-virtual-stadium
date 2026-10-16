// The streamed 3D: Google Photorealistic 3D Tiles for the city (Tileset_Context, ion asset 2275207) and the licensed
// CGTrader stadium, seats and NFL field from Cesium ion. With the stadium present the Google tiles are clipped to the
// level's CartoPoly_LFF_Footprint (ADR-0004); without it, Google's own stadium is the stand-in (CESIUM_PLACEMENT V1).
// Streaming follows ULFFTileStreamingSubsystem::ConfigureTileset: keep tiles around the camera, show parents, not holes.
import {
    Cartesian3, Cesium3DTileset, ClippingPolygon, ClippingPolygonCollection, Color, ColorGeometryInstanceAttribute, EllipseGeometry,
    GeometryInstance, IonResource, Math as CesiumMath, Matrix3, Matrix4, PerInstanceColorAppearance, Primitive, ShadowMode,
} from "cesium";
import type { Scene } from "cesium";
import type { DeploymentConfig } from "../config";
import type { TilesetRole } from "./look";
import type { Geo } from "./geo";

/** ADR-0004 registration: model (metres, Z-up, origin at midfield turf) into the site's map frame (E = site X, N = -site Y). */
const REGISTRATION = { thetaDeg: 182.25, offsetE: -2.25, offsetN: 2.5 };

export interface TileSet {
    context: Cesium3DTileset | null;
    stadium: Cesium3DTileset[];
    /** Every tileset's load progress settled (for the loading cover). */
    progress(): number;
    /** Why a stadium or city tileset is missing, in plain words (shown on screen: main.ts). */
    problems: string[];
}

/** A tileset load failure in plain words: no token, token rejected, no access to the asset, or no connection. */
function describe(error: unknown, what: string, id: number): string {
    const status = (error as { statusCode?: number } | null)?.statusCode;
    if (status === 401) return `${what}: the Cesium ion token in this build was rejected (check the token).`;
    if (status === 404 || status === 403) return `${what}: the Cesium ion token can't access asset ${id}.`;
    if (status === undefined) return `${what}: Cesium ion couldn't be reached (network).`;
    return `${what}: Cesium ion answered ${status}.`;
}

function configure(tileset: Cesium3DTileset): void {
    // ConfigureTileset: no holes (parents shown until children arrive). Unlike Unreal, the cache is capped per quality
    // tier (scene/look.ts), nothing loads while hidden, and requests the camera is flying past are dropped: on iPad,
    // memory runs out long before bandwidth.
    tileset.preloadFlightDestinations = true;
    tileset.preloadWhenHidden = false;
    tileset.cullRequestsWhileMoving = true;
    tileset.cullRequestsWhileMovingMultiplier = 60;
    tileset.skipLevelOfDetail = false;
    tileset.cacheBytes = 96 * 1024 * 1024;
    tileset.maximumCacheOverflowBytes = 64 * 1024 * 1024;
    tileset.foveatedScreenSpaceError = true;
    tileset.dynamicScreenSpaceError = true;
}

/** Where the registration puts the stadium model: ENU at the origin, turned and shifted as Unreal baked it. */
function registrationMatrix(geo: Geo, config: DeploymentConfig): Matrix4 {
    // The site's map frame is the ENU frame turned counter-clockwise by -(site yaw) (Unreal yaw turns East toward South).
    const siteYaw = geo.data.siteRoot.rotation.yaw;
    const mapToEnu = -siteYaw; // degrees CCW
    const theta = CesiumMath.toRadians(REGISTRATION.thetaDeg + mapToEnu + config.stadiumHeadingAdjustDeg);
    const c = Math.cos(CesiumMath.toRadians(mapToEnu)), s = Math.sin(CesiumMath.toRadians(mapToEnu));
    const e = c * REGISTRATION.offsetE - s * REGISTRATION.offsetN + config.stadiumOffsetAdjustM[0];
    const n = s * REGISTRATION.offsetE + c * REGISTRATION.offsetN + config.stadiumOffsetAdjustM[1];
    const local = Matrix4.fromRotationTranslation(Matrix3.fromRotationZ(theta), new Cartesian3(e, n, config.stadiumOffsetAdjustM[2]));
    return Matrix4.multiply(geo.enuToFixed, local, new Matrix4());
}

function placeStadium(tileset: Cesium3DTileset, geo: Geo, config: DeploymentConfig, label: string): void {
    const centre = tileset.boundingSphere.center;
    const distance = Cartesian3.distance(centre, geo.origin);
    const georeferenced = distance < 2000;
    const register = config.stadiumPlacement === "registration" || (config.stadiumPlacement === "auto" && !georeferenced);
    if (!register) {
        console.info(`${label}: using its own placement (${distance.toFixed(1)} m from midfield).`);
        return;
    }
    // Cancel whatever root transform ion gave it, then apply the Unreal registration on the model's own coordinates.
    const root = tileset.root.transform ?? Matrix4.IDENTITY;
    const inverseRoot = Matrix4.inverse(root, new Matrix4());
    tileset.modelMatrix = Matrix4.multiply(registrationMatrix(geo, config), inverseRoot, new Matrix4());
    const placed = Cartesian3.distance(tileset.boundingSphere.center, geo.origin);
    console.info(`${label}: placed ${(distance / 1000).toFixed(1)} km away; applied the ADR-0004 registration ` +
        `(now ${placed.toFixed(1)} m from midfield). Tune with ?stadiumHeading= and ?stadiumOffset=e,n,u if needed.`);
}

export async function loadTiles(scene: Scene, geo: Geo, config: DeploymentConfig, footprintLonLat: number[][],
    onTileset: (set: Cesium3DTileset, role: TilesetRole) => void = () => undefined): Promise<TileSet> {
    const stadium: Cesium3DTileset[] = [];
    let context: Cesium3DTileset | null = null;
    const problems: string[] = [];

    const stadiumToken = config.stadiumIonToken || config.ionToken;
    // The stadium structure, the seats and the field, in that order: from tileset URLs when given, else the ion assets.
    const roles: TilesetRole[] = ["structure", "seats", "field"];
    const urls = config.stadiumTilesetUrls;
    if (urls.length === 0 && !stadiumToken && config.stadiumAssetIds.length > 0) problems.push("Stadium: this build has no Cesium ion token.");
    const sources: { label: string; id: number; open: () => Promise<Cesium3DTileset> }[] = urls.length > 0
        ? urls.map((url, i) => ({ label: `Stadium tileset ${url}`, id: i, open: () => Cesium3DTileset.fromUrl(url, { maximumScreenSpaceError: 8 }) }))
        : stadiumToken ? config.stadiumAssetIds.map((id) => ({
            label: `Stadium asset ${id}`, id,
            open: async () => Cesium3DTileset.fromUrl(await IonResource.fromAssetId(id, { accessToken: stadiumToken }), { maximumScreenSpaceError: 8 }),
        })) : [];
    const loaded = await Promise.allSettled(sources.map(async (source, index) => {
        const tileset = await source.open();
        configure(tileset);
        placeStadium(tileset, geo, config, source.label);
        onTileset(tileset, roles[index] ?? "structure");
        scene.primitives.add(tileset);
        return tileset;
    }));
    loaded.forEach((result, i) => {
        if (result.status === "fulfilled") stadium.push(result.value);
        else {
            console.error(`${sources[i].label} could not be loaded.`, result.reason);
            problems.push(urls.length > 0 ? `${sources[i].label} could not be loaded.` : describe(result.reason, sources[i].label, sources[i].id));
        }
    });

    const contextToken = config.ionToken || stadiumToken;
    if (contextToken) {
        try {
            const resource = await IonResource.fromAssetId(config.googleTilesAssetId, { accessToken: contextToken });
            context = await Cesium3DTileset.fromUrl(resource, { showCreditsOnScreen: true, maximumScreenSpaceError: 16 });
            configure(context);
            if (stadium.length > 0 && footprintLonLat.length >= 3) {
                // The model supplies the stadium, plaza and lots; the tiles supply the city beyond.
                context.clippingPolygons = new ClippingPolygonCollection({
                    polygons: [new ClippingPolygon({ positions: Cartesian3.fromDegreesArray(footprintLonLat.flat()) })],
                });
            }
            onTileset(context, "city");
            scene.primitives.add(context);
        } catch (error) {
            console.error("Google Photorealistic 3D Tiles could not be loaded (check the ion token's asset access).", error);
            problems.push(describe(error, "City (Google 3D Tiles)", config.googleTilesAssetId));
        }
    } else {
        console.error("No Cesium ion token: set VITE_CESIUM_ION_TOKEN (web/.env.local) or open the page once with ?ionToken=.");
        problems.push("City (Google 3D Tiles): this build has no Cesium ion token.");
    }
    // Google's terrain is part of its tiles and the globe is off, so without them there would be no ground at all: a plain
    // disc at turf level keeps the level's stand-ins standing on something (only when the city tiles failed).
    if (context === null) {
        const turf = geo.data.originHeight - 0.05; // just under the lot paint (Unreal Z = 0)
        scene.primitives.add(new Primitive({
            geometryInstances: new GeometryInstance({
                geometry: new EllipseGeometry({ center: geo.origin, semiMajorAxis: 3000, semiMinorAxis: 3000, height: turf, vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT }),
                attributes: { color: ColorGeometryInstanceAttribute.fromColor(Color.fromCssColorString("#7d8a7a")) },
            }),
            appearance: new PerInstanceColorAppearance({ flat: false }),
            asynchronous: false,
            shadows: ShadowMode.RECEIVE_ONLY,
        }));
        console.warn("City tiles unavailable: showing a plain ground disc.");
    }
    const all = [context, ...stadium].filter((t): t is Cesium3DTileset => t !== null);
    let pending = 0;
    let seenLoading = false;
    for (const tileset of all) {
        tileset.loadProgress.addEventListener((requests: number, processing: number) => {
            pending = requests + processing;
            if (pending > 0) seenLoading = true;
        });
        tileset.allTilesLoaded.addEventListener(() => {
            pending = 0;
        });
    }
    return {
        context,
        stadium,
        problems,
        progress: () => {
            if (all.length === 0 || all.every((t) => t.tilesLoaded)) return 100;
            if (!seenLoading) return 0;
            return pending === 0 ? 100 : Math.max(0, 100 - pending * 2);
        },
    };
}
