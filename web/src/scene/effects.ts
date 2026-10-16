// The camera's post-process blendables, as CesiumJS post-process stages:
//   - M_LFF_LevelHighlight (SourceData/UI/Materials/LFF_LevelHighlight.hlsl): the picked level's height band shows through
//     a kelly tint inside the stadium's footprint, the rest of the stadium recedes a little. Works from depth alone.
//   - The Lights preview's exposure lift (ALFFCameraRig::SetLightPulse): the picture brightens by 2^stops, a slow sine.
import { Cartesian3, Matrix4, PostProcessStage } from "cesium";
import type { Scene } from "cesium";
import { GameMode } from "../config";
import { Vec3 } from "../util/ue";
import type { Geo } from "./geo";

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

void main() {
    vec4 colour = texture(colorTexture, v_textureCoordinates);
    float depth = czm_readDepth(depthTexture, v_textureCoordinates);
    if (depth >= 1.0) { out_FragColor = colour; return; }
    vec4 eye = czm_windowToEyeCoordinates(gl_FragCoord.xy, depth);
    vec3 L = (u_eyeToSite * vec4(eye.xyz / eye.w, 1.0)).xyz;
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

export class CameraEffects {
    private readonly highlight: PostProcessStage;
    private readonly exposure: PostProcessStage;
    private band: { minZ: number; maxZ: number } | null = null;
    private stops = 0;
    private readonly eyeToSite = new Matrix4();
    /** Earth-fixed -> site frame (cm, Unreal axes): ENU metres -> mirror Y -> centimetres -> inverse site rotation. */
    private readonly fixedToSite: Matrix4;

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
        scene.postProcessStages.add(this.highlight);
        scene.postProcessStages.add(this.exposure);
        // Double precision on the CPU, so the shader only sees eye-relative coordinates (no float jitter on the band).
        scene.preRender.addEventListener(() => {
            if (this.highlight.enabled) Matrix4.multiply(this.fixedToSite, scene.camera.inverseViewMatrix, this.eyeToSite);
        });
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
