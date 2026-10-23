// The picture's look, after Unreal's PPV_LFF_Cinematic, Fog_LFF_Aerial and Clouds_LFF_PartlyCloudy
// (Tools/Editor/setup_cinematic_look.py): HDR with a filmic (ACES) curve, exposure bias +0.3 EV, vignette 0.2, sun
// shadows, sky lighting on the models, contact shadows where the device can afford them, and a static partly cloudy sky.
// Unreal's bloom (0.35) is left out: CesiumJS's bloom haloes dark shapes against the bright sky instead of softly
// lifting highlights (Docs/WEB_PARITY.md W15). Comfort rules hold: no motion blur, lens flare, grain, fringe, or anything that moves or flashes.
// Lumen's bounce light has no real-time equivalent here; Tools/Pipeline/bake_stadium_lighting.py bakes it instead.
import {
    Cartesian2, Cartesian3, CloudCollection, Color, CumulusCloud, Math as CesiumMath, Matrix4, PostProcessStage, ShadowMode, Tonemapper,
} from "cesium";
import type { Cesium3DTileset, Scene, Viewer } from "cesium";
import type { Geo } from "./geo";
import type { QualitySettings } from "./quality";

/** The streamed tilesets: the stadium model's three parts (stadium structure, seats, field) and the Google city tiles. */
export type TilesetRole = "structure" | "seats" | "field" | "city";

/**
 * The coarsest the stadium model may get when its tile cache is full. CesiumJS keeps coarsening a tileset over budget
 * without limit, and the stadium's tiles don't thin out evenly: the structure still holds 300+ MB when its floors and
 * walls have already gone, and the seats vanish entirely from about 96. Up to 64 every surface stays; past that the
 * cache runs over its budget instead.
 */
const STADIUM_ERROR_CEILING = 64;

/** Unreal's look values (setup_cinematic_look.py POST). */
const UNREAL = { exposureBiasEv: 0.3, vignetteIntensity: 0.2 };
/** Clouds_LFF_PartlyCloudy: cumulus at 1.5 km, 3.5 km thick, about 40% of the sky. */
const CLOUDS = { bottomM: 1500, thicknessM: 2000, radiusM: 16000, count: 46, seed: 2026 };

const VIGNETTE_SHADER = /* glsl */ `
uniform sampler2D colorTexture;
uniform float u_intensity;
in vec2 v_textureCoordinates;
void main() {
    vec4 colour = texture(colorTexture, v_textureCoordinates);
    // Unreal's vignette: a soft radial darkening toward the corners, scaled by intensity.
    vec2 d = (v_textureCoordinates - 0.5) * vec2(1.0, 0.82);
    float falloff = smoothstep(0.25, 0.85, length(d) * 1.4142);
    out_FragColor = vec4(colour.rgb * (1.0 - u_intensity * falloff), colour.a);
}`;

function random(seed: number): () => number {
    let s = seed >>> 0;
    return () => {
        s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
        return s / 4294967296;
    };
}

export class SceneLook {
    private readonly vignette: PostProcessStage;
    private clouds: CloudCollection | null = null;
    private tilesets: { set: Cesium3DTileset; kind: TilesetRole }[] = [];
    private settings: QualitySettings | null = null;

    /** `bakedStadium`: the stadium's lighting is baked in, so it only casts shadows and ambient occlusion stays off. */
    constructor(private readonly viewer: Viewer, private readonly geo: Geo, private readonly bakedStadium = false) {
        const scene = viewer.scene;
        if (scene.highDynamicRangeSupported) scene.highDynamicRange = true;
        // Unreal's tonemapper is filmic and ACES-fitted.
        scene.postProcessStages.tonemapper = Tonemapper.ACES;
        scene.postProcessStages.exposure = Math.pow(2, UNREAL.exposureBiasEv);
        this.vignette = new PostProcessStage({
            name: "lff_vignette",
            fragmentShader: VIGNETTE_SHADER,
            uniforms: { u_intensity: UNREAL.vignetteIntensity },
        });
        scene.postProcessStages.add(this.vignette);
        // Gentle aerial haze on distant city tiles (Fog_LFF_Aerial starts at 300 m).
        scene.fog.enabled = true;
        scene.fog.density = 2.0e-4;
        scene.fog.minimumBrightness = 0.25;
        viewer.shadows = false;
        scene.preUpdate.addEventListener(() => this.limitCoarsening());
    }

    /** Holds each stadium tileset's memory-adjusted screen-space error at or under the ceiling (CesiumJS has no limit). */
    private limitCoarsening(): void {
        for (const { set, kind } of this.tilesets) {
            if (kind === "city") continue;
            const adjusted = set as unknown as { _memoryAdjustedScreenSpaceError: number };
            const ceiling = Math.max(STADIUM_ERROR_CEILING, set.maximumScreenSpaceError);
            if (adjusted._memoryAdjustedScreenSpaceError > ceiling) adjusted._memoryAdjustedScreenSpaceError = ceiling;
        }
    }

    /** The streamed tilesets: their sharpness follows the tier; the city's photographed lighting takes no extra shadows. */
    addTileset(set: Cesium3DTileset, kind: TilesetRole): void {
        this.tilesets.push({ set, kind });
        set.shadows = kind === "city" ? ShadowMode.DISABLED : this.bakedStadium ? ShadowMode.CAST_ONLY : ShadowMode.ENABLED;
        if (this.settings) this.applyTileset(set, kind, this.settings);
    }

    apply(settings: QualitySettings): void {
        this.settings = settings;
        const viewer = this.viewer, scene = viewer.scene;

        // Resolution: device pixels up to the tier's ratio (Cesium's default is CSS pixels, soft on a Retina iPad).
        const dpr = window.devicePixelRatio || 1;
        viewer.useBrowserRecommendedResolution = false;
        viewer.resolutionScale = Math.min(dpr, settings.pixelRatio) / dpr;
        if (scene.msaaSamples !== settings.msaaSamples && scene.msaaSupported) scene.msaaSamples = settings.msaaSamples;
        scene.postProcessStages.fxaa.enabled = settings.msaaSamples <= 1;

        // Shadows near the camera from the scene light: the sun at the step's time, or the floodlights after dark
        // (scene/timeOfDay.ts).
        viewer.shadows = settings.shadows;
        const shadowMap = scene.shadowMap;
        shadowMap.size = settings.shadowMapSize;
        shadowMap.softShadows = settings.softShadows;
        shadowMap.maximumDistance = 600;
        shadowMap.darkness = 0.35;
        shadowMap.fadingEnabled = true;

        const ao = scene.postProcessStages.ambientOcclusion;
        ao.enabled = settings.ambientOcclusion && !this.bakedStadium;
        if (ao.enabled) {
            Object.assign(ao.uniforms, { intensity: 2.2, bias: 0.12, lengthCap: 0.35, stepSize: 1.6, blurStepSize: 0.9, frustumLength: 400 });
        }

        scene.postProcessStages.bloom.enabled = false;

        if (settings.clouds && !this.clouds) this.clouds = this.makeClouds(scene);
        if (this.clouds) this.clouds.show = settings.clouds;

        for (const { set, kind } of this.tilesets) this.applyTileset(set, kind, settings);
        scene.requestRender();
    }

    private applyTileset(set: Cesium3DTileset, kind: TilesetRole, settings: QualitySettings): void {
        set.maximumScreenSpaceError = kind === "city" ? settings.cityError : settings.stadiumError;
        const budget = { structure: settings.structureCacheMB, seats: settings.seatsCacheMB, field: settings.fieldCacheMB, city: settings.cityCacheMB };
        set.cacheBytes = budget[kind] * 1024 * 1024;
        set.maximumCacheOverflowBytes = settings.cacheOverflowMB * 1024 * 1024;
    }

    /** A fixed, deterministic cumulus field around the stadium; nothing drifts (the noise never changes). */
    private makeClouds(scene: Scene): CloudCollection {
        const clouds = new CloudCollection({ noiseDetail: 16 });
        const rnd = random(CLOUDS.seed);
        for (let i = 0; i < CLOUDS.count; ++i) {
            // Spread by area, thinner overhead so the stadium bowl keeps clear sky above it.
            const r = CLOUDS.radiusM * Math.sqrt(0.08 + 0.92 * rnd());
            const a = rnd() * CesiumMath.TWO_PI;
            const width = 900 + rnd() * 1600;
            const height = width * (0.35 + rnd() * 0.2);
            const up = CLOUDS.bottomM + rnd() * (CLOUDS.thicknessM - height * 0.5) + height * 0.5;
            const enu = new Cartesian3(Math.cos(a) * r, Math.sin(a) * r, up - this.geo.data.originHeight);
            clouds.add({
                position: Matrix4.multiplyByPoint(this.geo.enuToFixed, enu, new Cartesian3()),
                scale: new Cartesian2(width, height),
                maximumSize: new Cartesian3(50, 16 + rnd() * 6, 12 + rnd() * 4),
                slice: 0.32 + rnd() * 0.2,
                brightness: 0.95 + rnd() * 0.1,
                color: Color.WHITE,
            } as unknown as CumulusCloud);
        }
        scene.primitives.add(clouds);
        return clouds;
    }
}
