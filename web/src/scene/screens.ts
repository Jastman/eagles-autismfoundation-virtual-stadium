// The stadium's LED displays, animated (web addition, Docs/WEB_PARITY.md W23): the two end-zone video boards (north
// 192 x 27 ft, south 160 x 27 ft) and the ribbon boards on the club-level fascia along both sidelines. Each is a canvas
// drawn in the browser and shown as an unlit, glowing panel in front of the model's own screen.
//
// Comfort: content changes by slow cross-fades and slides, nothing flashes or strobes, and with Calm Mode or less
// movement every display holds one still frame.
import { Appearance, GeometryInstance, Material, MaterialAppearance, Matrix4, PlaneGeometry, Primitive, VertexFormat } from "cesium";
import type { Scene } from "cesium";
import { Rotator, Transform, Vec3 } from "../util/ue";
import { ueTransformToLocalMatrix } from "./geo";
import type { Geo } from "./geo";

/** Site frame (cm; +X across the field to the east stand, +Y to the south end, +Z above the turf); yaw faces the field. */
interface DisplaySpec {
    id: string;
    kind: "board" | "ribbon";
    centre: Vec3;
    yaw: number;
    widthCm: number;
    heightCm: number;
}

const FT = 30.48;
const DISPLAYS: DisplaySpec[] = [
    // Above the end zones' upper decks (their back rows: north y -124 m at 40 m up, south y 122 m at 42 m up).
    { id: "north-board", kind: "board", centre: new Vec3(0, -12560, 4620), yaw: 90, widthCm: 192 * FT, heightCm: 27 * FT },
    { id: "south-board", kind: "board", centre: new Vec3(0, 12380, 4870), yaw: -90, widthCm: 160 * FT, heightCm: 27 * FT },
    // The club level's fascia, over the lower bowl's back rows (west x -74.6 m, east 70 m; club front 15.9 m up).
    { id: "west-ribbon", kind: "ribbon", centre: new Vec3(-6640, 0, 1330), yaw: 0, widthCm: 9000, heightCm: 110 },
    { id: "east-ribbon", kind: "ribbon", centre: new Vec3(6380, 0, 1330), yaw: 180, widthCm: 9000, heightCm: 110 },
];

/** What the displays say: the game the tour is set to. */
export interface DisplayGame {
    opponentShort: string;
    /** Three-letter code ("DAL"). */
    opponentCode: string;
    kickoffLabel: string;
    weekday: string;
    broadcast: string;
}

const MIDNIGHT = "#004C54";
const KELLY = "#69BE28";
const SILVER = "#A5ACAF";
const FRAME_SECONDS = 1 / 15;
const SCENE_SECONDS = 7;
const FADE_SECONDS = 0.8;

const SHADER = `
czm_material czm_getMaterial(czm_materialInput materialInput) {
    czm_material material = czm_getDefaultMaterial(materialInput);
    vec3 led = texture(image, materialInput.st).rgb;
    material.diffuse = vec3(0.0);
    material.emission = led * 1.15;
    material.alpha = 1.0;
    return material;
}`;

type Painter = (ctx: CanvasRenderingContext2D, w: number, h: number, t: number) => void;

class Display {
    readonly canvas = document.createElement("canvas");
    readonly primitive: Primitive;
    private readonly material: Material;

    constructor(scene: Scene, geo: Geo, readonly spec: DisplaySpec) {
        const aspect = spec.widthCm / spec.heightCm;
        this.canvas.width = spec.kind === "board" ? 1536 : 2048;
        this.canvas.height = spec.kind === "board" ? Math.round(1536 / aspect) : 40;
        const local = Transform.fromRotator(new Rotator(0, spec.yaw, 0), spec.centre);
        const world = local.mul(geo.siteFrame);
        const frame = ueTransformToLocalMatrix(world);
        // As the signs (scene/labels.ts): plane X -> +Y (width, reading left to right for the viewer), plane Y -> +Z,
        // normal -> +X (towards the field), half a metre in front of the structure.
        const basis = Matrix4.fromArray([
            0, spec.widthCm, 0, 0,
            0, 0, spec.heightCm, 0,
            1, 0, 0, 0,
            50, 0, 0, 1,
        ]);
        this.material = new Material({ fabric: { uniforms: { image: this.canvas }, source: SHADER } });
        this.primitive = new Primitive({
            geometryInstances: new GeometryInstance({
                geometry: new PlaneGeometry({ vertexFormat: VertexFormat.POSITION_NORMAL_AND_ST }),
                modelMatrix: Matrix4.multiply(frame, basis, new Matrix4()),
            }),
            appearance: new MaterialAppearance({
                material: this.material,
                translucent: false,
                flat: true,
                faceForward: false,
                closed: false,
                renderState: { cull: { enabled: true } },
            }) as Appearance,
            modelMatrix: Matrix4.clone(geo.enuToFixed),
            asynchronous: true,
            allowPicking: false,
        });
        scene.primitives.add(this.primitive);
    }

    /** Sends the canvas to its texture (Cesium only re-reads a canvas uniform when it is a different object). */
    upload(): void {
        const textures = (this.material as unknown as { _textures?: Record<string, { copyFrom?: (o: { source: HTMLCanvasElement }) => void; width: number; height: number }> })._textures;
        const texture = textures?.image;
        if (texture?.copyFrom && texture.width === this.canvas.width && texture.height === this.canvas.height) {
            texture.copyFrom({ source: this.canvas });
        }
    }
}

export class StadiumDisplays {
    private readonly displays: Display[];
    private elapsed = 0;
    private sinceFrame = FRAME_SECONDS;
    private still = false;
    private drewStill = false;
    private gameOn = false;
    private game: DisplayGame = { opponentShort: "Opponent", opponentCode: "OPP", kickoffLabel: "1:00 PM", weekday: "Sunday", broadcast: "" };

    constructor(scene: Scene, geo: Geo) {
        this.displays = DISPLAYS.map((spec) => new Display(scene, geo, spec));
        this.draw(0);
    }

    setGame(game: DisplayGame): void {
        this.game = game;
        this.drewStill = false;
        this.draw(this.elapsed);
    }

    /** After kickoff the boards show the score and clock; before it, the welcome and the matchup. */
    setGameOn(on: boolean): void {
        if (on === this.gameOn) return;
        this.gameOn = on;
        this.drewStill = false;
    }

    /** Calm Mode or less movement: one still frame on every display. */
    setStill(still: boolean): void {
        if (still === this.still) return;
        this.still = still;
        this.drewStill = false;
    }

    tick(dt: number): void {
        if (this.still) {
            if (!this.drewStill) {
                this.draw(0);
                this.drewStill = true;
            }
            return;
        }
        this.elapsed += dt;
        this.sinceFrame += dt;
        if (this.sinceFrame < FRAME_SECONDS) return;
        this.sinceFrame = 0;
        this.draw(this.elapsed);
    }

    private draw(t: number): void {
        for (const display of this.displays) {
            const ctx = display.canvas.getContext("2d")!;
            const { width: w, height: h } = display.canvas;
            if (display.spec.kind === "board") this.paintBoard(ctx, w, h, t, display.spec.id === "south-board");
            else this.paintRibbon(ctx, w, h, t, display.spec.id === "east-ribbon");
            display.upload();
        }
    }

    // ---- video boards: a slow carousel of full-width scenes, cross-faded ----

    private boardScenes(): Painter[] {
        const g = this.game;
        const welcome: Painter = (ctx, w, h, t) => {
            stripes(ctx, w, h, t);
            text(ctx, "WELCOME TO", w / 2, h * 0.3, h * 0.17, SILVER);
            text(ctx, "LINCOLN FINANCIAL FIELD", w / 2, h * 0.62, h * 0.3, "#fff");
        };
        const matchup: Painter = (ctx, w, h, t) => {
            stripes(ctx, w, h, t);
            text(ctx, `${g.opponentShort.toUpperCase()}  AT  EAGLES`, w / 2, h * 0.4, h * 0.32, "#fff");
            text(ctx, `${g.weekday.toUpperCase()}  ·  KICKOFF ${g.kickoffLabel}${g.broadcast ? `  ·  ${g.broadcast.toUpperCase()}` : ""}`, w / 2, h * 0.78, h * 0.13, KELLY);
        };
        const fly: Painter = (ctx, w, h, t) => {
            waves(ctx, w, h, t);
            text(ctx, "FLY EAGLES FLY", w / 2, h * 0.54, h * 0.42, "#fff", t);
        };
        const eaf: Painter = (ctx, w, h, t) => {
            ctx.fillStyle = "#fff";
            ctx.fillRect(0, 0, w, h);
            bars(ctx, w * 0.08, h * 0.22, w * 0.2, h * 0.56, t);
            text(ctx, "EAGLES AUTISM FOUNDATION", w * 0.62, h * 0.4, h * 0.2, MIDNIGHT);
            text(ctx, "AWARENESS TO ACTION", w * 0.62, h * 0.7, h * 0.15, KELLY);
        };
        const score: Painter = (ctx, w, h, t) => {
            stripes(ctx, w, h, t * 0.5);
            const cell = (label: string, value: string, x: number, colour: string) => {
                text(ctx, label, x, h * 0.3, h * 0.17, SILVER);
                text(ctx, value, x, h * 0.66, h * 0.38, colour);
            };
            cell(g.opponentCode, "0", w * 0.18, "#fff");
            cell("PHI", "0", w * 0.4, KELLY);
            cell("QTR", "1ST", w * 0.62, "#fff");
            cell("CLOCK", "15:00", w * 0.84, "#fff");
        };
        return this.gameOn ? [score, fly, eaf, score, matchup] : [welcome, matchup, fly, eaf];
    }

    private paintBoard(ctx: CanvasRenderingContext2D, w: number, h: number, t: number, offset: boolean): void {
        const scenes = this.boardScenes();
        const clock = t + (offset ? SCENE_SECONDS / 2 : 0);
        const index = Math.floor(clock / SCENE_SECONDS) % scenes.length;
        const into = clock % SCENE_SECONDS;
        ctx.save();
        ctx.globalAlpha = 1;
        scenes[index](ctx, w, h, clock);
        // Cross-fade into the next scene over the last FADE_SECONDS: no cut, no flash.
        const fade = Math.max(0, (into - (SCENE_SECONDS - FADE_SECONDS)) / FADE_SECONDS);
        if (fade > 0) {
            ctx.globalAlpha = fade;
            scenes[(index + 1) % scenes.length](ctx, w, h, clock);
        }
        ctx.restore();
        frame(ctx, w, h);
    }

    // ---- ribbon boards: a slow marquee ----

    private paintRibbon(ctx: CanvasRenderingContext2D, w: number, h: number, t: number, reverse: boolean): void {
        ctx.fillStyle = "#031a1d";
        ctx.fillRect(0, 0, w, h);
        const items = ["GO BIRDS", "FLY EAGLES FLY", "EAGLES AUTISM FOUNDATION", `EAGLES VS. ${this.game.opponentShort.toUpperCase()}`, "WELCOME TO THE LINC"];
        ctx.font = `700 ${Math.round(h * 0.7)}px "Atkinson Hyperlegible", system-ui, sans-serif`;
        ctx.textBaseline = "middle";
        const gap = h * 2.2;
        const widths = items.map((s) => ctx.measureText(s).width + gap);
        const total = widths.reduce((a, b) => a + b, 0);
        const speed = 70; // px per second: slow enough to read, never a blur
        let x = -(((reverse ? -t : t) * speed) % total);
        if (x > 0) x -= total;
        while (x < w) {
            items.forEach((s, i) => {
                ctx.fillStyle = i % 2 === 0 ? "#fff" : KELLY;
                ctx.fillText(s, x, h * 0.55);
                ctx.fillStyle = SILVER;
                ctx.beginPath();
                ctx.arc(x + widths[i] - gap / 2, h * 0.5, h * 0.12, 0, Math.PI * 2);
                ctx.fill();
                x += widths[i];
            });
        }
    }
}

function text(ctx: CanvasRenderingContext2D, s: string, x: number, y: number, size: number, colour: string, sweepT?: number): void {
    ctx.font = `700 ${Math.round(size)}px "Atkinson Hyperlegible", system-ui, sans-serif`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillStyle = colour;
    ctx.fillText(s, x, y);
    if (sweepT !== undefined) {
        // A soft light sweep across the lettering, once every few seconds.
        const width = ctx.measureText(s).width;
        const p = ((sweepT * 0.25) % 1.4) - 0.2;
        const stop = (v: number) => Math.min(Math.max(v, 0), 1);
        const g = ctx.createLinearGradient(x - width / 2, 0, x + width / 2, 0);
        g.addColorStop(stop(p - 0.12), "rgba(255,255,255,0)");
        g.addColorStop(stop(p), "rgba(200,255,170,0.55)");
        g.addColorStop(stop(p + 0.12), "rgba(255,255,255,0)");
        ctx.globalCompositeOperation = "source-atop";
        ctx.fillStyle = g;
        ctx.fillText(s, x, y);
        ctx.globalCompositeOperation = "source-over";
    }
}

/** Midnight green with slow diagonal bands drifting across. */
function stripes(ctx: CanvasRenderingContext2D, w: number, h: number, t: number): void {
    const g = ctx.createLinearGradient(0, 0, w, h);
    g.addColorStop(0, "#003238");
    g.addColorStop(0.5, MIDNIGHT);
    g.addColorStop(1, "#002a2f");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = "rgba(255,255,255,0.05)";
    const step = h * 1.2;
    const shift = (t * 30) % step;
    for (let x = -h * 2 + shift; x < w + h; x += step) {
        ctx.beginPath();
        ctx.moveTo(x, h);
        ctx.lineTo(x + h * 0.5, h);
        ctx.lineTo(x + h * 1.5, 0);
        ctx.lineTo(x + h, 0);
        ctx.fill();
    }
}

/** Kelly and midnight waves rolling slowly under the text. */
function waves(ctx: CanvasRenderingContext2D, w: number, h: number, t: number): void {
    ctx.fillStyle = "#00262b";
    ctx.fillRect(0, 0, w, h);
    const layers = [{ c: "rgba(0,76,84,0.9)", a: 0.12, k: 1.6, s: 0.5, y: 0.62 }, { c: "rgba(105,190,40,0.35)", a: 0.08, k: 2.4, s: -0.35, y: 0.78 }];
    for (const l of layers) {
        ctx.fillStyle = l.c;
        ctx.beginPath();
        ctx.moveTo(0, h);
        for (let x = 0; x <= w; x += 16) ctx.lineTo(x, h * (l.y + l.a * Math.sin((x / w) * Math.PI * 2 * l.k + t * l.s)));
        ctx.lineTo(w, h);
        ctx.fill();
    }
}

/** The foundation's rising bars, filling in one by one and holding. */
function bars(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, t: number): void {
    const colours = ["#4BFF5A", "#52E064", "#55CB70", "#57B676", "#53A276", "#4E9373", "#448970", "#37796B", "#2A6B66", "#044F57"];
    const n = colours.length;
    const gap = w * 0.02;
    const bw = (w - gap * (n - 1)) / n;
    const shown = Math.min(n, Math.floor(((t % SCENE_SECONDS) / 2.5) * n) + 1);
    for (let i = 0; i < shown; ++i) {
        const bh = h * (0.3 + 0.7 * (i / (n - 1)));
        ctx.fillStyle = colours[i];
        ctx.fillRect(x + i * (bw + gap), y + h - bh, bw, bh);
    }
}

/** The LED cabinet seams, so the panel reads as a screen. */
function frame(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    ctx.strokeStyle = "rgba(0,0,0,0.35)";
    ctx.lineWidth = 2;
    for (let x = w / 16; x < w; x += w / 16) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h);
        ctx.stroke();
    }
    ctx.strokeStyle = "#0b0b0c";
    ctx.lineWidth = 6;
    ctx.strokeRect(3, 3, w - 6, h - 6);
}
