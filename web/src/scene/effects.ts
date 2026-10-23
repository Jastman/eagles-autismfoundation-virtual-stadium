// The camera's post-process blendables, as CesiumJS post-process stages:
//   - M_LFF_LevelHighlight (SourceData/UI/Materials/LFF_LevelHighlight.hlsl): the picked level's height band shows through
//     a kelly tint inside the stadium's footprint, the rest of the stadium recedes a little. Works from depth alone.
//   - The Lights preview's exposure lift (ALFFCameraRig::SetLightPulse): the picture brightens by 2^stops, a slow sine.
//   - Evening and night (web addition, Docs/WEB_PARITY.md W22): the photographed city and the stadium model carry
//     daylight in their textures, so after sunset the picture is relit by where each pixel is: the bowl under the
//     stadium's floodlights, the outside of the stadium in their spill, the plaza and lots under warm lamps, the city dim.
//     The sky is left to Cesium's atmosphere at the real time, with its stars and moon.
import { Cartesian3, Matrix4, PostProcessStage } from "cesium";
import type { Scene } from "cesium";
import { GameMode } from "../config";
import { Vec3 } from "../util/ue";
import type { Geo } from "./geo";

// Eye coordinates (m) of this pixel, as CesiumJS's own ambient occlusion rebuilds them: czm_readDepth has already undone
// the logarithmic depth, so the window-to-eye helper (which undoes it again) would put everything hundreds of metres off.
const EYE_POSITION = /* glsl */ `
vec3 eyePosition(float depth) {
    vec4 eye = czm_inverseProjection * vec4(2.0 * v_textureCoordinates - 1.0, depth, 1.0);
    return eye.xyz / eye.w;
}`;

const HIGHLIGHT_SHADER = /* glsl */ `
uniform sampler2D colorTexture;
uniform sampler2D depthTexture;
uniform mat4 u_eyeToSite;      // eye coordinates (m) -> site frame (cm, Unreal axes)
uniform vec2 u_band;           // BandMin, BandMax (cm above the turf)
uniform vec2 u_footprint;      // FootprintX, FootprintY (cm)
uniform vec3 u_tint;           // HighlightColor, linear
uniform float u_opacity;       // HighlightOpacity
uniform float u_dim;           // DimOutside
in vec2 v_textureCoordinates;
${EYE_POSITION}

void main() {
    vec4 colour = texture(colorTexture, v_textureCoordinates);
    float depth = czm_readDepth(depthTexture, v_textureCoordinates);
    if (depth >= 1.0) { out_FragColor = colour; return; }
    vec3 L = (u_eyeToSite * vec4(eyePosition(depth), 1.0)).xyz;
    // Inside the stadium's footprint (an ellipse), feathered over its last 8%.
    float E = length(vec2(L.x / max(u_footprint.x, 1.0), L.y / max(u_footprint.y, 1.0)));
    float inside = 1.0 - smoothstep(0.92, 1.0, E);
    // The picked level's band, feathered over 40 cm at each end.
    float band = smoothstep(u_band.x - 40.0, u_band.x + 40.0, L.z) * (1.0 - smoothstep(u_band.y - 40.0, u_band.y + 40.0, L.z));
    float inLevel = inside * band;
    float outLevel = inside * (1.0 - band);
    vec3 C = colour.rgb;
    float grey = dot(C, vec3(0.3, 0.59, 0.11));
    C = mix(C, mix(vec3(grey), C, 0.5) * 0.7, u_dim * outLevel);
    vec3 tint = pow(clamp(u_tint, 0.0, 1.0), vec3(1.0 / 2.2));
    C = mix(C, clamp(C * 0.6 + tint * 0.9, 0.0, 1.0), u_opacity * inLevel);
    out_FragColor = vec4(C, colour.a);
}`;

const EXPOSURE_SHADER = /* glsl */ `
uniform sampler2D colorTexture;
uniform float u_gain;
in vec2 v_textureCoordinates;
void main() {
    vec4 colour = texture(colorTexture, v_textureCoordinates);
    out_FragColor = vec4(min(colour.rgb * u_gain, vec3(1.0)), colour.a);
}`;

const NIGHT_SHADER = /* glsl */ `
uniform sampler2D colorTexture;
uniform sampler2D depthTexture;
uniform mat4 u_eyeToSite;      // eye coordinates (m) -> site frame (cm: X across the field, Y along it, Z above the turf)
uniform vec2 u_footprint;      // the stadium's footprint radii (cm)
uniform float u_night;         // 0 day .. 1 night
uniform float u_golden;        // low-sun warmth
uniform float u_floods;        // the stadium's lights: 0 off .. 1 on
in vec2 v_textureCoordinates;
${EYE_POSITION}

void main() {
    vec4 colour = texture(colorTexture, v_textureCoordinates);
    float depth = czm_readDepth(depthTexture, v_textureCoordinates);
    // The sky is Cesium's atmosphere at the real time of day.
    if (depth >= 1.0) { out_FragColor = colour; return; }
    vec3 L = (u_eyeToSite * vec4(eyePosition(depth), 1.0)).xyz;
    float E = length(vec2(L.x / u_footprint.x, L.y / u_footprint.y));
    float belowRoof = 1.0 - smoothstep(6200.0, 8500.0, L.z);
    // The bowl, stands and field under the floodlights; the field a little brighter.
    float bowl = (1.0 - smoothstep(0.9, 1.0, E)) * belowRoof;
    float field = 1.0 - smoothstep(0.42, 0.58, E);
    // The stadium's outside: spill from the bowl, lit concourses and uplights on the facade.
    float facade = smoothstep(0.9, 1.0, E) * (1.0 - smoothstep(1.02, 1.25, E)) * belowRoof;
    // The plaza and lots: warm lamps in pools near the ground, out to the edge of the lots.
    float metres = length(L.xy) / 100.0;
    float site = 1.0 - smoothstep(320.0, 700.0, metres);
    float ground = 1.0 - smoothstep(900.0, 3200.0, L.z);
    vec2 cell = fract(L.xy / 2600.0) - 0.5;
    float pool = 1.0 - smoothstep(0.12, 0.5, length(cell));
    float lamps = site * ground * (0.32 + 0.4 * pool) * (1.0 - bowl);
    // The rest of the city: moonlight, sky glow and street lights low down.
    vec3 moon = vec3(0.07, 0.085, 0.12);
    vec3 street = vec3(0.13, 0.09, 0.05) * (1.0 - smoothstep(1500.0, 6000.0, L.z));
    vec3 flood = vec3(0.98, 1.0, 1.04) * u_floods;
    vec3 lamp = vec3(1.0, 0.76, 0.5);
    vec3 light = moon + street + lamp * lamps + flood * (bowl * (0.92 + 0.14 * field) + facade * 0.42);
    vec3 C = mix(colour.rgb, colour.rgb * light, u_night);
    C *= mix(vec3(1.0), vec3(1.07, 0.94, 0.8), u_golden);
    out_FragColor = vec4(C, colour.a);
}`;

export class CameraEffects {
    private readonly highlight: PostProcessStage;
    private readonly exposure: PostProcessStage;
    private readonly nightGrade: PostProcessStage;
    private band: { minZ: number; maxZ: number } | null = null;
    private stops = 0;
    private night = { night: 0, golden: 0, floods: 1 };
    private readonly eyeToSite = new Matrix4();
    /** Earth-fixed -> site frame (cm, Unreal axes): ENU metres -> mirror Y -> centimetres -> inverse site rotation. */
    readonly fixedToSite: Matrix4;

    constructor(private readonly scene: Scene, geo: Geo) {
        const site = geo.siteFrame;
        // world (cm, Unreal) -> site: inverse of the site root's rigid transform, written as a matrix in Unreal axes.
        const ex = site.rotation.unrotateVector(new Vec3(1, 0, 0));
        const ey = site.rotation.unrotateVector(new Vec3(0, 1, 0));
        const ez = site.rotation.unrotateVector(new Vec3(0, 0, 1));
        const t = site.rotation.unrotateVector(site.translation.scale(-1));
        const worldToSite = Matrix4.fromArray([ex.x, ex.y, ex.z, 0, ey.x, ey.y, ey.z, 0, ez.x, ez.y, ez.z, 0, t.x, t.y, t.z, 1]);
        // ENU metres -> Unreal world cm: (e, n, u) -> (100 e, -100 n, 100 u).
        const enuToWorld = Matrix4.fromScale(new Cartesian3(100, -100, 100));
        this.fixedToSite = Matrix4.multiply(worldToSite, Matrix4.multiply(enuToWorld, geo.fixedToEnu, new Matrix4()), new Matrix4());

        this.highlight = new PostProcessStage({
            name: "lff_level_highlight",
            fragmentShader: HIGHLIGHT_SHADER,
            uniforms: {
                u_eyeToSite: () => this.eyeToSite,
                u_band: () => ({ x: this.band?.minZ ?? 0, y: this.band?.maxZ ?? 0 }),
                u_footprint: () => ({ x: GameMode.StadiumFootprintRadii.x, y: GameMode.StadiumFootprintRadii.y }),
                u_tint: () => ({ x: GameMode.LevelHighlightColor[0], y: GameMode.LevelHighlightColor[1], z: GameMode.LevelHighlightColor[2] }),
                u_opacity: () => GameMode.LevelHighlightOpacity,
                u_dim: () => GameMode.LevelDimOutside,
            },
        });
        this.highlight.enabled = false;
        this.exposure = new PostProcessStage({
            name: "lff_light_pulse",
            fragmentShader: EXPOSURE_SHADER,
            uniforms: { u_gain: () => Math.pow(2, this.stops) },
        });
        this.exposure.enabled = false;
        this.nightGrade = new PostProcessStage({
            name: "lff_night",
            fragmentShader: NIGHT_SHADER,
            uniforms: {
                u_eyeToSite: () => this.eyeToSite,
                u_footprint: () => ({ x: GameMode.StadiumFootprintRadii.x, y: GameMode.StadiumFootprintRadii.y }),
                u_night: () => this.night.night,
                u_golden: () => this.night.golden,
                u_floods: () => this.night.floods,
            },
        });
        this.nightGrade.enabled = false;
        // Relit first, so the level highlight and the light pulse work on the evening picture.
        // One depth frustum (near 0.1 m to far 1e10 m): the stages rebuild positions with one projection, so a second
        // frustum's pixels would land in the wrong place. Logarithmic depth keeps the precision.
        scene.logarithmicDepthFarToNearRatio = Math.max(scene.logarithmicDepthFarToNearRatio, 1e11);
        scene.postProcessStages.add(this.nightGrade);
        scene.postProcessStages.add(this.highlight);
        scene.postProcessStages.add(this.exposure);
        // Double precision on the CPU, so the shader only sees eye-relative coordinates (no float jitter on the band).
        scene.preRender.addEventListener(() => {
            if (this.highlight.enabled || this.nightGrade.enabled) Matrix4.multiply(this.fixedToSite, scene.camera.inverseViewMatrix, this.eyeToSite);
        });
    }

    /** The time of day's relighting (scene/timeOfDay.ts); off in full daylight. */
    setNightGrade(night: number, golden: number, floods: number): void {
        this.night = { night, golden, floods };
        this.nightGrade.enabled = night > 1e-3 || golden > 1e-3;
    }

    setLevelHighlight(band: { minZ: number; maxZ: number } | null): void {
        this.band = band;
        this.highlight.enabled = band !== null;
        this.scene.requestRender();
    }

    setExposureStops(stops: number): void {
        this.stops = stops;
        this.exposure.enabled = stops > 1e-4;
    }
}
