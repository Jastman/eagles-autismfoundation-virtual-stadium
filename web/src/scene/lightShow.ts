// The cell phone light show of a night game (the 8:15 PM template's "light-show" step; web addition, Docs/WEB_PARITY.md
// W22): small phone lights across the stands, each one slowly brightening and dimming on its own, with a gentle wave
// travelling round the bowl. Nothing strobes. Less movement holds them steady; Calm Mode and Crowd Off hide them.
import { Color, PointPrimitiveCollection } from "cesium";
import type { PointPrimitive, Scene } from "cesium";
import { RandomStream } from "./crowd";
import { Vec3 } from "../util/ue";
import type { Geo } from "./geo";

const PHONES = 3200;
/** Phone height above the seat's floor (held up at shoulder height), cm. */
const HELD_CM = 150;

interface Phone {
    point: PointPrimitive;
    rate: number;
    phase: number;
    angle: number;
}

export class PhoneLightShow {
    private readonly collection = new PointPrimitiveCollection();
    private phones: Phone[] = [];
    private shown = false;
    private animate = true;
    private t = 0;

    constructor(scene: Scene, private readonly geo: Geo) {
        this.collection.show = false;
        scene.primitives.add(this.collection);
    }

    async load(): Promise<void> {
        try {
            const seats = (await fetch("media/crowd/seats.json").then((r) => r.json())) as { seats: number[][] };
            const random = new RandomStream(826);
            const all = seats.seats;
            for (let i = 0; i < PHONES && all.length > 0; ++i) {
                const seat = all[random.randHelper(all.length)];
                const site = new Vec3(seat[0], seat[1], seat[2] + HELD_CM);
                const position = this.geo.worldToFixed(this.geo.siteToWorld(site));
                const point = this.collection.add({ position, pixelSize: 3, color: Color.WHITE, disableDepthTestDistance: 0 });
                this.phones.push({ point, rate: 0.18 + random.fraction() * 0.35, phase: random.fraction() * Math.PI * 2, angle: Math.atan2(seat[1], seat[0]) });
            }
            this.apply();
        } catch (error) {
            console.warn("Light show: seats could not be loaded.", error);
        }
    }

    /** Shown on its step; animate false holds every light steady (less movement). */
    set(shown: boolean, animate: boolean): void {
        if (shown === this.shown && animate === this.animate) return;
        this.shown = shown;
        this.animate = animate;
        this.collection.show = shown;
        this.apply();
    }

    tick(dt: number): void {
        if (!this.shown || !this.animate) return;
        this.t += dt;
        this.apply();
    }

    private apply(): void {
        if (!this.shown) return;
        for (const p of this.phones) {
            let level = 0.85;
            if (this.animate) {
                const own = 0.5 + 0.5 * Math.sin(this.t * p.rate * Math.PI * 2 + p.phase);
                // A soft wave round the bowl, about one lap every 12 seconds.
                const wave = 0.5 + 0.5 * Math.cos(p.angle - this.t * 0.52);
                level = 0.25 + 0.5 * own * own + 0.35 * wave * wave;
            }
            p.point.color = new Color(0.92 + 0.08 * level, 0.95 + 0.05 * level, 1, Math.min(level, 1));
            p.point.pixelSize = 2.2 + 2.2 * level;
        }
    }
}
