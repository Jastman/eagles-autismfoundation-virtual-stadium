// The time of day each step is shown at (web addition, Docs/WEB_PARITY.md W22). Every kickoff template gives its steps a
// time (journey/gameday.ts); this sets Cesium's clock to that moment on the game's date, so the sky, sun, moon and
// stars are the real ones over the stadium, and relights the picture for it: low warm sun before sunset, then the
// stadium under its floodlights, the plaza under lamps and the city dark (scene/effects.ts). A 1:00 PM tour stays in
// daylight; an 8:15 PM tour goes from late afternoon through sunset into night.
//
// The change between steps is a slow cross-fade (never a cut or a flash); the stadium's light banks fade on at dusk.
import { BillboardCollection, Cartesian3, Color, DirectionalLight, DynamicAtmosphereLightingType, JulianDate, Matrix4 } from "cesium";
import type { Billboard, Viewer } from "cesium";
import { Vec3 } from "../util/ue";
import type { CameraEffects } from "./effects";
import type { FloodlightShading } from "./floodShading";
import type { Geo } from "./geo";
import { daylightLevels, sunPosition } from "./solar";

const RAD = Math.PI / 180;
/** Seconds a change of time takes (with less movement, still a fade, only shorter). */
const CHANGE_SECONDS = 2.5;
const CHANGE_SECONDS_REDUCED = 1.2;
/** The longest change of time that fades (one game day's steps are within it). */
const MAX_FADE_MS = 6 * 3600 * 1000;

/**
 * The floodlight banks (site frame, cm), from the bowl in Content/LFF/Crowd/seats.json: along both sideline canopies
 * above the upper deck's back rows (x about +-111 m, 47 m up), and over each end's upper deck.
 */
export function lightBanks(): Vec3[] {
    const banks: Vec3[] = [];
    for (let y = -8400; y <= 8400; y += 700) {
        banks.push(new Vec3(-11500, y, 5450), new Vec3(11050, y, 5450));
    }
    for (let x = -4200; x <= 4200; x += 700) {
        banks.push(new Vec3(x, -12800, 4950), new Vec3(x, 12600, 5200));
    }
    return banks;
}

function glowImage(): HTMLCanvasElement {
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = 64;
    const ctx = canvas.getContext("2d")!;
    const g = ctx.createRadialGradient(32, 32, 0, 32, 32, 32);
    g.addColorStop(0, "rgba(255,255,255,1)");
    g.addColorStop(0.18, "rgba(250,252,255,0.95)");
    g.addColorStop(0.45, "rgba(220,235,255,0.35)");
    g.addColorStop(1, "rgba(200,220,255,0)");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 64, 64);
    return canvas;
}

export class TimeOfDay {
    private from = 0;
    private to = 0;
    private now = 0;
    private elapsed = 0;
    private seconds = 0;
    private readonly light = new DirectionalLight({ direction: new Cartesian3(0, 0, -1), intensity: 2 });
    private readonly banks: BillboardCollection;
    private readonly cores: Billboard[] = [];
    private readonly halos: Billboard[] = [];
    night = 0;
    golden = 0;
    /** The light banks' glow, 0..1 (they read once the sky dims). */
    glow = 0;
    /** False when the three.js layer draws the banks (scene/threeLayer.ts). */
    private banksShown = true;

    constructor(private readonly viewer: Viewer, private readonly geo: Geo, private readonly effects: CameraEffects,
        private readonly flood: FloodlightShading) {
        viewer.scene.light = this.light;
        // The sky is lit by the real sun at the clock's time (the model light above is the scene light, not the sun).
        viewer.scene.atmosphere.dynamicLighting = DynamicAtmosphereLightingType.SUNLIGHT;
        this.banks = new BillboardCollection({ scene: viewer.scene });
        const image = glowImage();
        for (const site of lightBanks()) {
            const position = geo.worldToFixed(geo.siteToWorld(site));
            this.halos.push(this.banks.add({ position, image, sizeInMeters: true, width: 16, height: 9, color: new Color(0.85, 0.92, 1, 0), show: false }));
            this.cores.push(this.banks.add({ position, image, sizeInMeters: true, width: 4.5, height: 2.6, color: new Color(1, 1, 1, 0), show: false }));
        }
        viewer.scene.primitives.add(this.banks);
    }

    /** Moves to a moment (UTC ms): at once, or with a slow cross-fade. */
    setTime(utcMs: number, fade: "instant" | "fade" | "reduced"): void {
        if (utcMs === this.to && this.now !== 0) return;
        // Another game's date is a cut, never a fade: a fade across days would run through days and nights in seconds.
        if (fade === "instant" || this.now === 0 || Math.abs(utcMs - this.now) > MAX_FADE_MS) {
            this.from = this.to = this.now = utcMs;
            this.seconds = 0;
            this.apply(utcMs);
            return;
        }
        this.from = this.now;
        this.to = utcMs;
        this.elapsed = 0;
        this.seconds = fade === "reduced" ? CHANGE_SECONDS_REDUCED : CHANGE_SECONDS;
    }

    tick(dt: number): void {
        if (this.now === this.to) return;
        this.elapsed += dt;
        const t = Math.min(this.elapsed / Math.max(this.seconds, 1e-3), 1);
        const eased = t * t * (3 - 2 * t);
        this.now = t >= 1 ? this.to : this.from + (this.to - this.from) * eased;
        this.apply(this.now);
    }

    private apply(utcMs: number): void {
        const viewer = this.viewer;
        viewer.clock.currentTime = JulianDate.fromDate(new Date(utcMs));
        const sun = sunPosition(utcMs, this.geo.data.originLatitude, this.geo.data.originLongitude);
        const { night, golden } = daylightLevels(sun.altitude);
        this.night = night;
        this.golden = golden;

        // Shading for the models: the sun while it is up (never from below the horizon, which nothing here blocks), then
        // the floodlights from high over the bowl. Night's darkness itself is the post-process relight.
        const sunAlt = Math.max(sun.altitude, 8) * RAD;
        const sunDir = new Cartesian3(Math.cos(sunAlt) * Math.sin(sun.azimuth * RAD), Math.cos(sunAlt) * Math.cos(sun.azimuth * RAD), Math.sin(sunAlt));
        const floodDir = new Cartesian3(0.15, -0.1, 1);
        const towardLight = Cartesian3.normalize(Cartesian3.lerp(Cartesian3.normalize(sunDir, sunDir), Cartesian3.normalize(floodDir, floodDir), night, new Cartesian3()), new Cartesian3());
        const direction = Matrix4.multiplyByPointAsVector(this.geo.enuToFixed, Cartesian3.negate(towardLight, towardLight), new Cartesian3());
        this.light.direction = Cartesian3.normalize(direction, direction);
        const warm = golden * 0.8;
        this.light.color = new Color(1, 1 - 0.12 * warm, 1 - 0.3 * warm, 1);
        this.light.intensity = 2 - 0.4 * night;

        // The stadium's lights are on for the game; they read once the sky dims.
        const floods = 1;
        this.effects.setNightGrade(night, golden, floods);
        const glow = Math.min(Math.max((night - 0.15) / 0.6, 0), 1);
        this.glow = glow;
        this.flood.setNight(night);
        for (let i = 0; i < this.cores.length; ++i) {
            this.cores[i].show = this.halos[i].show = this.banksShown && glow > 0.01;
            this.cores[i].color = new Color(1, 1, 1, glow);
            this.halos[i].color = new Color(0.85, 0.92, 1, 0.38 * glow);
        }
        viewer.scene.requestRender();
    }

    /** The three.js layer draws the banks with bloom instead of these billboards. */
    setBanksShown(shown: boolean): void {
        this.banksShown = shown;
        if (this.now !== 0) this.apply(this.now);
    }
}
