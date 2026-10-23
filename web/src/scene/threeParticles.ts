// GPU particles in the three.js layer (scene/threeLayer.ts, Docs/WEB_PARITY.md W24): the night game's phone light show
// and the fireworks and gate flares. Both are animated entirely in shaders from one clock, so thousands of points cost
// one draw call each. Comfort rules as the Cesium versions they replace: slow, no strobe, no flicker; still with less
// movement; the cue system decides when fireworks play (never in Calm Mode).
import { AdditiveBlending, BufferAttribute, BufferGeometry, Group, Points, ShaderMaterial, Vector3 } from "three";
import type { PerspectiveCamera } from "three";
import { RandomStream } from "./crowd";
import type { FireworksCue } from "./cues";
import { Vec3 } from "../util/ue";
import type { Geo } from "./geo";
import type { LayerPart } from "./threeLayer";
import { siteToEnu, worldToEnu } from "./threeLayer";

// ---- the phone light show ----

const PHONE_VS = /* glsl */ `
attribute float aPhase;
attribute float aRate;
attribute float aAngle;
uniform float u_time;
uniform float u_animate;
uniform float u_pixelScale;
varying float vLevel;
void main() {
    float own = 0.5 + 0.5 * sin(u_time * aRate * 6.2831853 + aPhase);
    // A soft wave round the bowl, about one lap every 12 seconds.
    float wave = 0.5 + 0.5 * cos(aAngle - u_time * 0.52);
    float moving = 0.22 + 0.5 * own * own + 0.38 * wave * wave;
    vLevel = mix(0.8, moving, u_animate);
    vec4 mv = modelViewMatrix * vec4(position, 1.0);
    gl_Position = projectionMatrix * mv;
    // A phone's light, a little larger than life so it reads across the bowl.
    gl_PointSize = clamp(0.5 * u_pixelScale / max(-mv.z, 1.0), 1.5, 7.0) * (0.75 + 0.45 * vLevel);
}`;

const PHONE_FS = /* glsl */ `
varying float vLevel;
void main() {
    vec2 c = gl_PointCoord - 0.5;
    float a = exp(-dot(c, c) * 18.0) * vLevel;
    gl_FragColor = vec4(vec3(0.93, 0.96, 1.0) * a * 1.3, 1.0);
    #include <colorspace_fragment>
}`;

const PHONES = 6000;
/** Phone height above the seat's floor (held up at shoulder height), cm. */
const HELD_CM = 150;

export class PhoneLightsLayer implements LayerPart {
    readonly object: Points;
    private readonly material: ShaderMaterial;
    private shown = false;
    private animate = true;
    private time = 0;

    constructor() {
        this.material = new ShaderMaterial({
            vertexShader: PHONE_VS, fragmentShader: PHONE_FS, transparent: true, depthWrite: false, depthTest: false, blending: AdditiveBlending,
            uniforms: { u_time: { value: 0 }, u_animate: { value: 1 }, u_pixelScale: { value: 800 } },
        });
        this.object = new Points(new BufferGeometry(), this.material);
        this.object.frustumCulled = false;
    }

    async load(geo: Geo): Promise<void> {
        try {
            const seats = (await fetch("media/crowd/seats.json").then((r) => r.json())) as { seats: number[][] };
            const random = new RandomStream(826);
            const n = Math.min(PHONES, seats.seats.length);
            const position = new Float32Array(n * 3), phase = new Float32Array(n), rate = new Float32Array(n), angle = new Float32Array(n);
            for (let i = 0; i < n; ++i) {
                const seat = seats.seats[random.randHelper(seats.seats.length)];
                siteToEnu(geo, new Vec3(seat[0], seat[1], seat[2] + HELD_CM)).toArray(position, i * 3);
                phase[i] = random.fraction() * Math.PI * 2;
                rate[i] = 0.18 + random.fraction() * 0.35;
                angle[i] = Math.atan2(seat[1], seat[0]);
            }
            const g = this.object.geometry;
            g.setAttribute("position", new BufferAttribute(position, 3));
            g.setAttribute("aPhase", new BufferAttribute(phase, 1));
            g.setAttribute("aRate", new BufferAttribute(rate, 1));
            g.setAttribute("aAngle", new BufferAttribute(angle, 1));
        } catch (error) {
            console.warn("Light show: seats could not be loaded.", error);
        }
    }

    /** As scene/lightShow.ts: shown on its step; animate false holds every light steady. */
    set(shown: boolean, animate: boolean): void {
        this.shown = shown;
        this.animate = animate;
    }

    active(): boolean {
        return this.shown;
    }

    update(dt: number, _camera: PerspectiveCamera, pixelScale: number): void {
        if (this.animate) this.time += dt;
        this.material.uniforms.u_time.value = this.time;
        this.material.uniforms.u_animate.value = this.animate ? 1 : 0;
        this.material.uniforms.u_pixelScale.value = pixelScale;
    }
}

// ---- fireworks and gate flares: the cue system's bursts, drawn as sparks with short trails ----

const SPARK_VS = /* glsl */ `
attribute vec3 aDir;
attribute float aSpeed;
attribute float aLag;
uniform vec3 u_centre;
uniform float u_t;
uniform float u_radius;
uniform float u_drop;
uniform float u_size;
uniform float u_pixelScale;
varying float vAlpha;
void main() {
    // The spark's own moment: trail points are a little behind it.
    float t = u_t - aLag;
    // Opens quickly then slows (ease-out), drifts down, and shrinks away in the second half. Nothing flashes.
    float open = 1.0 - pow(1.0 - clamp(t / 0.55, 0.0, 1.0), 3.0);
    vec3 p = u_centre + aDir * u_radius * open * aSpeed - vec3(0.0, 0.0, u_drop * t * t);
    float grow = smoothstep(0.0, 0.1, t);
    float shrink = 1.0 - smoothstep(0.45, 1.0, t);
    vAlpha = (t > 0.0 ? 1.0 : 0.0) * grow * shrink * (1.0 - aLag * 9.0);
    vec4 mv = modelViewMatrix * vec4(p, 1.0);
    gl_Position = projectionMatrix * mv;
    gl_PointSize = clamp(u_size * u_pixelScale / max(-mv.z, 1.0), 1.0, 26.0) * (1.0 - aLag * 6.0);
}`;

const SPARK_FS = /* glsl */ `
uniform vec3 u_colour;
varying float vAlpha;
void main() {
    vec2 c = gl_PointCoord - 0.5;
    float a = exp(-dot(c, c) * 14.0) * vAlpha;
    if (a < 0.003) discard;
    gl_FragColor = vec4(u_colour * a * 1.6, 1.0);
    #include <colorspace_fragment>
}`;

const SPARKS = 150;
const TRAIL = [0, 0.025, 0.05, 0.075];

class Burst {
    readonly points: Points;
    readonly material: ShaderMaterial;

    constructor(seed: number) {
        const random = new RandomStream(seed);
        const n = SPARKS * TRAIL.length;
        const dir = new Float32Array(n * 3), speed = new Float32Array(n), lag = new Float32Array(n), position = new Float32Array(n * 3);
        const golden = Math.PI * (3 - Math.sqrt(5));
        for (let i = 0; i < SPARKS; ++i) {
            const z = 1 - (2 * (i + 0.5)) / SPARKS;
            const ring = Math.sqrt(Math.max(0, 1 - z * z));
            const a = golden * i + random.fraction() * 0.3;
            const s = 0.82 + random.fraction() * 0.3;
            TRAIL.forEach((l, k) => {
                const j = i * TRAIL.length + k;
                dir.set([Math.cos(a) * ring, Math.sin(a) * ring, z], j * 3);
                speed[j] = s;
                lag[j] = l;
            });
        }
        const g = new BufferGeometry();
        g.setAttribute("position", new BufferAttribute(position, 3));
        g.setAttribute("aDir", new BufferAttribute(dir, 3));
        g.setAttribute("aSpeed", new BufferAttribute(speed, 1));
        g.setAttribute("aLag", new BufferAttribute(lag, 1));
        this.material = new ShaderMaterial({
            vertexShader: SPARK_VS, fragmentShader: SPARK_FS, transparent: true, depthWrite: false, depthTest: false, blending: AdditiveBlending,
            uniforms: {
                u_centre: { value: new Vector3() }, u_t: { value: 0 }, u_radius: { value: 12 }, u_drop: { value: 3.5 }, u_size: { value: 1.2 },
                u_pixelScale: { value: 800 }, u_colour: { value: new Vector3(1, 0.8, 0.45) },
            },
        });
        this.points = new Points(g, this.material);
        this.points.frustumCulled = false;
    }
}

export class FireworksLayer implements LayerPart {
    readonly object = new Group();
    private readonly bursts: Burst[] = [];

    constructor(private readonly cues: () => FireworksCue[]) {}

    private states() {
        return this.cues().flatMap((cue) => cue.burstStates());
    }

    active(): boolean {
        return this.states().length > 0;
    }

    update(_dt: number, _camera: PerspectiveCamera, pixelScale: number): void {
        const states = this.states();
        while (this.bursts.length < states.length) {
            const burst = new Burst(1000 + this.bursts.length * 17);
            this.bursts.push(burst);
            this.object.add(burst.points);
        }
        this.bursts.forEach((burst, i) => {
            const s = states[i];
            burst.points.visible = s !== undefined;
            if (!s) return;
            const u = burst.material.uniforms;
            (u.u_centre.value as Vector3).copy(worldToEnu(s.centre));
            u.u_t.value = s.t;
            u.u_radius.value = s.radiusCm / 100;
            u.u_drop.value = s.dropCm / 100;
            u.u_size.value = (s.sparkSizeCm / 100) * 0.9;
            (u.u_colour.value as Vector3).set(s.colour[0], s.colour[1], s.colour[2]);
            u.u_pixelScale.value = pixelScale;
        });
    }
}
