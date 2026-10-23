// The stadium model lit by its own floodlights at night (Docs/WEB_PARITY.md W24): a CesiumJS CustomShader on the stadium,
// seats and field tilesets that adds the light of the four rows of light banks (both sideline canopies and both ends) to
// every surface by its normal and distance, so the stands facing the field glow, the field is evenly bright and surfaces
// turned away stay darker. The post-process relight (scene/effects.ts) still sets the overall night level; this gives it
// direction. Off in daylight (the shader returns at once).
import { CustomShader, Matrix4, UniformType } from "cesium";
import type { Cesium3DTileset, Scene } from "cesium";

const FLOOD_GLSL = /* glsl */ `
// A row of light banks as a line light (site frame, cm): Lambert towards the nearest point on the row, soft falloff.
float rowLight(vec3 P, vec3 N, vec3 a, vec3 b) {
    vec3 ab = b - a;
    float h = clamp(dot(P - a, ab) / dot(ab, ab), 0.0, 1.0);
    vec3 toLight = a + ab * h - P;
    float d = length(toLight);
    float lambert = max(dot(N, toLight / max(d, 1.0)), 0.0);
    return lambert / (1.0 + pow(d / 9000.0, 2.0));
}

void fragmentMain(FragmentInput fsInput, inout czm_modelMaterial material) {
    if (u_night < 0.001) return;
    vec3 P = (u_eyeToSite * vec4(fsInput.attributes.positionEC, 1.0)).xyz;
    // The site frame is a mirrored, scaled rotation of eye space: its own matrix carries normals correctly once normalised.
    vec3 N = normalize(mat3(u_eyeToSite) * material.normalEC);
    float E = length(vec2(P.x / 13000.0, P.y / 14000.0));
    float near = 1.0 - smoothstep(1.0, 1.35, E);
    float flood = rowLight(P, N, vec3(-11500.0, -8400.0, 5450.0), vec3(-11500.0, 8400.0, 5450.0))
        + rowLight(P, N, vec3(11050.0, -8400.0, 5450.0), vec3(11050.0, 8400.0, 5450.0))
        + 0.6 * rowLight(P, N, vec3(-4200.0, -12800.0, 4950.0), vec3(4200.0, -12800.0, 4950.0))
        + 0.6 * rowLight(P, N, vec3(-4200.0, 12600.0, 5200.0), vec3(4200.0, 12600.0, 5200.0));
    material.emissive += material.diffuse * vec3(0.95, 0.98, 1.04) * flood * near * u_night * 0.85;
}`;

export class FloodlightShading {
    readonly shader = new CustomShader({
        uniforms: {
            u_eyeToSite: { type: UniformType.MAT4, value: new Matrix4() },
            u_night: { type: UniformType.FLOAT, value: 0 },
        },
        fragmentShaderText: FLOOD_GLSL,
    });
    private readonly eyeToSite = new Matrix4();
    private night = 0;

    /** `fixedToSite`: Earth-fixed metres -> site frame cm (scene/effects.ts). */
    constructor(scene: Scene, private readonly fixedToSite: Matrix4) {
        // Double precision on the CPU each frame, so the shader only works in eye-relative coordinates.
        scene.preRender.addEventListener(() => {
            if (this.night <= 0) return;
            Matrix4.multiply(this.fixedToSite, scene.camera.inverseViewMatrix, this.eyeToSite);
            this.shader.setUniform("u_eyeToSite", this.eyeToSite);
        });
    }

    setNight(night: number): void {
        if (night === this.night) return;
        this.night = night;
        this.shader.setUniform("u_night", night);
    }

    apply(tileset: Cesium3DTileset): void {
        tileset.customShader = this.shader;
    }
}
