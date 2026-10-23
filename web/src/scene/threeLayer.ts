// A three.js layer over the CesiumJS picture for light and particles (Docs/WEB_PARITY.md W24, Docs/WEB_RENDERING_ROADMAP.md):
// the floodlight banks' glare and beams, the phone light show and the fireworks and gate flares, with bloom. Everything
// here gives off light and nothing else, so it is drawn on black in its own canvas and screen-blended over Cesium's: the
// glow adds to the picture without a second copy of the city or stadium. Its camera is Cesium's, copied after every frame.
//
// It has no depth from Cesium, so nothing here should sit where the stadium hides it: the light banks and fireworks are
// above the roof line, the light show is seen from a seat, and the beams below the rim only show from inside the bowl.
// Comfort: no strobe, no flicker; the light show and fireworks move slowly and stop with less movement.
import { Matrix4 as CesiumMatrix4, Cartesian3 } from "cesium";
import type { PerspectiveFrustum, Scene } from "cesium";
import {
    AdditiveBlending, BufferAttribute, BufferGeometry, CylinderGeometry, DoubleSide, Group, Mesh, PerspectiveCamera,
    Points, Quaternion, Scene as ThreeScene, ShaderMaterial, SRGBColorSpace, Vector2, Vector3, WebGLRenderer,
} from "three";
import { EffectComposer } from "three/addons/postprocessing/EffectComposer.js";
import { OutputPass } from "three/addons/postprocessing/OutputPass.js";
import { RenderPass } from "three/addons/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/addons/postprocessing/UnrealBloomPass.js";
import { Vec3 } from "../util/ue";
import type { Geo } from "./geo";
import type { QualityTier } from "./quality";

/** Site frame (cm) -> the layer's frame: east-north-up metres at the georeference origin. */
export function siteToEnu(geo: Geo, site: Vec3): Vector3 {
    const w = geo.siteToWorld(site);
    return new Vector3(w.x / 100, -w.y / 100, w.z / 100);
}

export function worldToEnu(w: Vec3): Vector3 {
    return new Vector3(w.x / 100, -w.y / 100, w.z / 100);
}

/** Something drawn in the layer: updated each frame, and whether it has anything to show. */
export interface LayerPart {
    readonly object: Group | Mesh | Points;
    update(dt: number, camera: PerspectiveCamera, pixelScale: number): void;
    active(): boolean;
}

// The threshold keeps the faint beam haze out of the bloom; the light banks, sparks and phones glow.
const BLOOM = { strength: 1.0, radius: 0.5, threshold: 0.32 };
/** Above the turf, the top of the stands and canopy (the upper deck's back rows are 47 m up): beams show above it from outside. */
const RIM_HEIGHT_M = 46;

export class ThreeLayer {
    readonly canvas: HTMLCanvasElement;
    private readonly renderer: WebGLRenderer;
    private readonly scene = new ThreeScene();
    readonly camera = new PerspectiveCamera(60, 16 / 9, 0.5, 40000);
    private readonly composer: EffectComposer;
    private readonly bloom: UnrealBloomPass;
    private readonly parts: LayerPart[] = [];
    private pixelCap = 1.5;
    private bloomOn = true;
    private lost = false;
    private shown = false;
    private last = performance.now();

    /** Null when the browser can't give the layer its own WebGL context; the Cesium-only effects stay then. */
    static create(container: HTMLElement, cesium: Scene, geo: Geo): ThreeLayer | null {
        try {
            return new ThreeLayer(container, cesium, geo);
        } catch (error) {
            console.warn("three.js layer unavailable; using the Cesium-only lights and particles.", error);
            return null;
        }
    }

    private constructor(container: HTMLElement, private readonly cesium: Scene, private readonly geo: Geo) {
        this.canvas = document.createElement("canvas");
        this.canvas.className = "three-layer";
        this.renderer = new WebGLRenderer({ canvas: this.canvas, antialias: false, alpha: false, powerPreference: "high-performance" });
        this.renderer.setClearColor(0x000000, 1);
        this.renderer.outputColorSpace = SRGBColorSpace;
        this.camera.up.set(0, 0, 1);
        this.composer = new EffectComposer(this.renderer);
        this.composer.addPass(new RenderPass(this.scene, this.camera));
        this.bloom = new UnrealBloomPass(new Vector2(256, 256), BLOOM.strength, BLOOM.radius, BLOOM.threshold);
        this.composer.addPass(this.bloom);
        this.composer.addPass(new OutputPass());
        this.canvas.addEventListener("webglcontextlost", (event) => {
            event.preventDefault();
            this.lost = true;
            this.canvas.style.display = "none";
            console.warn("three.js layer: GPU context lost; lights and particles off for this visit.");
        });
        this.canvas.style.display = "none";
        container.append(this.canvas);
        this.resize();
        window.addEventListener("resize", () => this.resize());
        // After Cesium has drawn the frame, with the camera it drew it from.
        cesium.postRender.addEventListener(() => this.render());
    }

    add(part: LayerPart): void {
        this.parts.push(part);
        this.scene.add(part.object);
    }

    /** Quality tiers (W16): bloom and resolution step down with the rest of the picture. */
    setQuality(tier: QualityTier): void {
        this.pixelCap = tier === "high" ? 1.5 : tier === "medium" ? 1.0 : 0.75;
        this.bloomOn = tier !== "low";
        this.bloom.enabled = this.bloomOn;
        this.resize();
    }

    private resize(): void {
        const dpr = Math.min(window.devicePixelRatio || 1, this.pixelCap);
        const w = window.innerWidth, h = window.innerHeight;
        this.renderer.setPixelRatio(dpr);
        this.renderer.setSize(w, h, false);
        this.composer.setPixelRatio(dpr);
        this.composer.setSize(w, h);
        // Bloom at half resolution: soft anyway, and a quarter of the cost.
        this.bloom.resolution.set(Math.max(64, (w * dpr) / 2), Math.max(64, (h * dpr) / 2));
    }

    private syncCamera(): void {
        const cam = this.cesium.camera;
        const toEnu = this.geo.fixedToEnu;
        const p = CesiumMatrix4.multiplyByPoint(toEnu, cam.positionWC, new Cartesian3());
        const d = CesiumMatrix4.multiplyByPointAsVector(toEnu, cam.directionWC, new Cartesian3());
        const u = CesiumMatrix4.multiplyByPointAsVector(toEnu, cam.upWC, new Cartesian3());
        this.camera.position.set(p.x, p.y, p.z);
        this.camera.up.set(u.x, u.y, u.z);
        this.camera.lookAt(p.x + d.x, p.y + d.y, p.z + d.z);
        const frustum = cam.frustum as PerspectiveFrustum;
        const fovy = frustum.fovy ?? Math.PI / 3;
        this.camera.fov = (fovy * 180) / Math.PI;
        this.camera.aspect = frustum.aspectRatio && Number.isFinite(frustum.aspectRatio) ? frustum.aspectRatio : window.innerWidth / window.innerHeight;
        this.camera.updateProjectionMatrix();
    }

    private render(): void {
        const now = performance.now();
        const dt = Math.min((now - this.last) / 1000, 0.1);
        this.last = now;
        if (this.lost) return;
        try {
            const anything = this.parts.some((p) => p.active());
            if (anything !== this.shown) {
                this.shown = anything;
                this.canvas.style.display = anything ? "" : "none";
            }
            if (!anything) return;
            this.syncCamera();
            const pixelScale = (this.renderer.getPixelRatio() * window.innerHeight) / (2 * Math.tan((this.camera.fov * Math.PI) / 360));
            for (const part of this.parts) {
                part.object.visible = part.active();
                if (part.object.visible) part.update(dt, this.camera, pixelScale);
            }
            if (this.bloomOn) this.composer.render(dt);
            else this.renderer.render(this.scene, this.camera);
        } catch (error) {
            // Scenery only: never take the stadium down with it.
            console.error("three.js layer failed; turning it off.", error);
            this.lost = true;
            this.canvas.style.display = "none";
        }
    }
}

// ---- the floodlights: glare on each light bank and its beam down into the bowl ----

const GLARE_VS = /* glsl */ `
uniform float u_pixelScale;
uniform float u_size;
void main() {
    vec4 mv = modelViewMatrix * vec4(position, 1.0);
    gl_Position = projectionMatrix * mv;
    gl_PointSize = clamp(u_size * u_pixelScale / max(-mv.z, 1.0), 2.0, 90.0);
}`;

const GLARE_FS = /* glsl */ `
uniform float u_level;
void main() {
    vec2 c = gl_PointCoord - 0.5;
    float r2 = dot(c, c);
    float core = exp(-r2 * 90.0);
    float halo = exp(-r2 * 14.0) * 0.35;
    // A soft horizontal streak, like a camera sees a light bank. Steady: nothing flickers.
    float streak = exp(-abs(c.y) * 70.0) * exp(-abs(c.x) * 4.5) * 0.45;
    float a = (core + halo + streak) * u_level;
    gl_FragColor = vec4(vec3(0.92, 0.96, 1.0) * a, 1.0);
    #include <colorspace_fragment>
}`;

const BEAM_VS = /* glsl */ `
varying float vAlong;
varying vec3 vNormal;
varying vec3 vView;
varying float vHeight;
varying float vDistance;
uniform float u_length;
void main() {
    // The cylinder runs along +Y from -L/2 (the far end) to +L/2 (the light).
    vAlong = 0.5 - position.y / u_length;
    vec4 world = modelMatrix * vec4(position, 1.0);
    vHeight = world.z;
    vNormal = normalize(mat3(modelMatrix) * normal);
    vView = normalize(cameraPosition - world.xyz);
    vDistance = length(cameraPosition - world.xyz);
    gl_Position = projectionMatrix * viewMatrix * world;
}`;

const BEAM_FS = /* glsl */ `
varying float vAlong;
varying vec3 vNormal;
varying vec3 vView;
varying float vHeight;
varying float vDistance;
uniform float u_level;
uniform float u_clipHeight;
void main() {
    if (vHeight < u_clipHeight) discard;
    // Thin haze lit by the beam: brightest at the light, fading with distance, soft at the cone's edges.
    float along = pow(1.0 - clamp(vAlong, 0.0, 1.0), 1.6) * smoothstep(0.0, 0.06, vAlong);
    float edge = pow(abs(dot(normalize(vNormal), normalize(vView))), 1.8);
    // Faint: seen down the field many beams overlap. Gone close to the camera, so none crosses the view as a wall.
    float near = smoothstep(25.0, 70.0, vDistance);
    float a = along * edge * near * u_level * 0.03;
    gl_FragColor = vec4(vec3(0.85, 0.92, 1.0) * a, 1.0);
    #include <colorspace_fragment>
}`;

export interface FloodlightBank {
    /** Site frame, cm. */
    at: Vec3;
    /** Where the bank is aimed, site frame cm. */
    aim: Vec3;
}

/** The light banks (as scene/timeOfDay.ts lightBanks) and where each is aimed: the near half of the field. */
export function aimBanks(banks: Vec3[]): FloodlightBank[] {
    return banks.map((at) => {
        const sideline = Math.abs(at.x) > 9000;
        const aim = sideline ? new Vec3(at.x * 0.18, at.y * 0.75, 0) : new Vec3(at.x * 0.6, at.y * 0.32, 0);
        return { at, aim };
    });
}

export class Floodlights implements LayerPart {
    readonly object = new Group();
    private readonly glare: ShaderMaterial;
    private readonly beams: ShaderMaterial[] = [];
    private level = 0;
    private beamsShown = true;

    constructor(private readonly geo: Geo, banks: FloodlightBank[]) {
        const positions = new Float32Array(banks.length * 3);
        banks.forEach((b, i) => siteToEnu(geo, b.at).toArray(positions, i * 3));
        const geometry = new BufferGeometry();
        geometry.setAttribute("position", new BufferAttribute(positions, 3));
        this.glare = new ShaderMaterial({
            vertexShader: GLARE_VS, fragmentShader: GLARE_FS, transparent: true, depthWrite: false, depthTest: false, blending: AdditiveBlending,
            uniforms: { u_pixelScale: { value: 800 }, u_size: { value: 5.5 }, u_level: { value: 0 } },
        });
        this.object.add(new Points(geometry, this.glare));

        // One beam for every other bank: enough to read as light in the air without a wall of cones.
        banks.filter((_, i) => i % 2 === 0).forEach((b) => {
            const from = siteToEnu(geo, b.at);
            const to = siteToEnu(geo, b.aim);
            const length = from.distanceTo(to);
            const material = new ShaderMaterial({
                vertexShader: BEAM_VS, fragmentShader: BEAM_FS, transparent: true, depthWrite: false, depthTest: false, side: DoubleSide,
                blending: AdditiveBlending,
                uniforms: { u_level: { value: 0 }, u_length: { value: length }, u_clipHeight: { value: -1e6 } },
            });
            const cone = new Mesh(new CylinderGeometry(0.8, length * Math.tan((11 * Math.PI) / 180), length, 20, 1, true), material);
            // Cylinder +Y towards the light, centred halfway along the beam.
            cone.quaternion.copy(new Quaternion().setFromUnitVectors(new Vector3(0, 1, 0), from.clone().sub(to).normalize()));
            cone.position.copy(from.clone().add(to).multiplyScalar(0.5));
            this.beams.push(material);
            this.object.add(cone);
        });
    }

    /** 0 (off) .. 1: the lights read once the sky dims (scene/timeOfDay.ts). */
    setLevel(level: number): void {
        this.level = level;
    }

    active(): boolean {
        return this.level > 0.01;
    }

    update(_dt: number, camera: PerspectiveCamera, pixelScale: number): void {
        this.glare.uniforms.u_level.value = this.level;
        this.glare.uniforms.u_pixelScale.value = pixelScale;
        // From outside the bowl, the stands and roof hide the beams below the rim: show only what is above it.
        const site = this.geo.siteFrame.inverseTransformPositionNoScale(new Vec3(camera.position.x * 100, -camera.position.y * 100, camera.position.z * 100));
        const inside = Math.hypot(site.x / 13000, site.y / 14000) < 0.92 && site.z < 6000;
        const clip = inside ? -1e6 : RIM_HEIGHT_M;
        for (const m of this.beams) {
            m.uniforms.u_level.value = this.beamsShown ? this.level : 0;
            m.uniforms.u_clipHeight.value = clip;
        }
    }

    setBeamsShown(shown: boolean): void {
        this.beamsShown = shown;
    }
}
