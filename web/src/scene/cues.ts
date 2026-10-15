// Ports of ALFFCueActor and ALFFFireworksActor (Source/VirtualStadium/Private/World/LFFCueActor.cpp, LFFFireworksActor.cpp).
// A cue is placed at its end pose (open, parked, end of the flight); it starts at StartOffset from there and moves back
// to it once, never looping. Calm Mode and less movement skip the move (the journey asks for the end state).
import { BillboardCollection, Cartesian3, Color, Matrix4 } from "cesium";
import type { Billboard, Scene } from "cesium";
import type { CueActor, CueTrigger } from "../journey/world";
import type { CueState } from "../journey/types";
import { Quat, Transform, Vec3, clamp, smoothStep } from "../util/ue";
import type { Geo } from "./geo";
import type { PropPrimitive } from "./props";
import type { CueData } from "./sceneData";
import { linearToSrgb, toTransform } from "./sceneData";

/** Something drawn with the cue: moved and hidden with it. */
export interface CueVisual {
    setWorld(world: Transform): void;
    setShown(shown: boolean): void;
    /** Seconds the cue has been moving (for the eagle's wing beat and walkers' steps). */
    animate?(movingSeconds: number, playing: boolean): void;
}

export class PrimitiveVisual implements CueVisual {
    private readonly matrix = new Matrix4();
    constructor(private readonly geo: Geo, private readonly prop: PropPrimitive) {}
    setWorld(world: Transform): void {
        this.geo.worldTransformToModelMatrix(world, this.matrix);
        this.prop.setModelMatrix(this.matrix);
    }
    setShown(shown: boolean): void {
        this.prop.setShown(shown);
    }
}

export class LevelCue implements CueActor {
    readonly cueId: string;
    readonly trigger: CueTrigger;
    readonly spectacle: boolean;
    readonly isFireworks: boolean = false;
    playDelaySeconds: number;
    playSeconds: number;
    protected readonly rest: Transform;
    protected readonly start: Transform;
    protected playing = false;
    protected elapsed = 0;
    protected hidden = false;
    protected current: Transform;
    readonly visuals: CueVisual[] = [];

    constructor(readonly label: string, protected readonly data: CueData, restWorld: Transform) {
        this.cueId = data.id;
        this.trigger = data.trigger;
        this.spectacle = data.spectacle;
        this.playDelaySeconds = data.playDelaySeconds;
        this.playSeconds = data.playSeconds;
        this.rest = restWorld;
        this.start = toTransform(data.start);
        this.start.scale3D = restWorld.scale3D.clone();
        this.current = restWorld.clone();
    }

    /** Before: start pose. After: end pose. Play: start pose, waiting for play(). Stops any move in progress. */
    applyState(state: CueState): void {
        this.playing = false;
        this.onStopped();
        const after = state === "After";
        this.applyAlpha(after ? 1 : 0, 0);
        this.setCueHidden(after ? this.data.hiddenAfter : this.data.hiddenBefore);
    }

    /** Runs the move once. Instant jumps straight to the end pose. */
    play(instant: boolean): void {
        if (instant) {
            this.applyState("After");
            return;
        }
        this.elapsed = 0;
        this.playing = true;
        this.applyAlpha(0, 0);
        // Something that appears for its move (the eagle) stays hidden through the delay.
        this.setCueHidden(this.data.hiddenBefore && this.playDelaySeconds > 0);
        this.onPlayStarted();
    }

    tick(dt: number): void {
        for (const visual of this.visuals) visual.animate?.(Math.max(this.elapsed - this.playDelaySeconds, 0), this.playing);
        if (!this.playing) return;
        this.elapsed += dt;
        const moving = this.elapsed - this.playDelaySeconds;
        if (moving < 0) return;
        this.setCueHidden(false);
        const linear = clamp(moving / Math.max(this.playSeconds, 0.01), 0, 1);
        // Ease-out cubic: it arrives gently and settles, with no bounce or overshoot.
        const alpha = this.data.linear ? linear : 1 - Math.pow(1 - linear, 3);
        this.applyAlpha(alpha, moving);
        if (linear >= 1) this.finish();
    }

    private finish(): void {
        this.playing = false;
        this.onStopped();
        this.applyAlpha(1, this.playSeconds);
        this.setCueHidden(this.data.hiddenAfter);
    }

    protected applyAlpha(alpha: number, _elapsedSeconds: number): void {
        const location = Vec3.lerp(this.start.translation, this.rest.translation, alpha);
        const rotation = Quat.slerp(this.start.rotation, this.rest.rotation, alpha);
        this.current = new Transform(rotation, location, this.rest.scale3D);
        for (const visual of this.visuals) visual.setWorld(this.current);
    }

    protected setCueHidden(hidden: boolean): void {
        this.hidden = hidden;
        for (const visual of this.visuals) visual.setShown(!hidden);
    }

    protected onPlayStarted(): void {}
    protected onStopped(): void {}

    isPlaying(): boolean {
        return this.playing;
    }

    isHidden(): boolean {
        return this.hidden;
    }

    totalSeconds(): number {
        return this.playDelaySeconds + this.playSeconds;
    }

    getLocation(): Vec3 {
        return this.current.translation;
    }

    getQuat(): Quat {
        return this.current.rotation;
    }
}

const SPHERE_DIAMETER = 100;
/** How far a spark falls over a burst, cm (a slow drift, not a drop). */
const SPARK_DROP = 350;

function sparkImage(): HTMLCanvasElement {
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = 64;
    const ctx = canvas.getContext("2d")!;
    // A round, evenly lit spark: the colour comes from the billboard. Steady, so it never flickers.
    const g = ctx.createRadialGradient(32, 32, 0, 32, 32, 32);
    g.addColorStop(0, "rgba(255,255,255,1)");
    g.addColorStop(0.7, "rgba(255,255,255,0.95)");
    g.addColorStop(1, "rgba(255,255,255,0)");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 64, 64);
    return canvas;
}

/** Soft fireworks: slow bursts of glowing sparks that open, drift down and shrink away. Nothing flashes. */
export class FireworksCue extends LevelCue {
    override readonly isFireworks = true;
    private readonly collection: BillboardCollection;
    private readonly bursts: Billboard[][] = [];
    private readonly directions: Vec3[] = [];

    constructor(label: string, data: CueData, restWorld: Transform, private readonly geo: Geo, scene: Scene) {
        super(label, data, restWorld);
        const f = data.fireworks!;
        const golden = Math.PI * (3 - Math.sqrt(5));
        for (let i = 0; i < f.sparksPerBurst; ++i) {
            const z = 1 - (2 * (i + 0.5)) / f.sparksPerBurst;
            const ring = Math.sqrt(Math.max(0, 1 - z * z));
            this.directions.push(new Vec3(Math.cos(golden * i) * ring, Math.sin(golden * i) * ring, z));
        }
        this.collection = new BillboardCollection({ scene });
        scene.primitives.add(this.collection);
        const image = sparkImage();
        f.burstOffsets.forEach((_, index) => {
            const c = f.burstColours.length > 0 ? f.burstColours[index % f.burstColours.length] : [1, 0.78, 0.45];
            const colour = new Color(linearToSrgb(c[0]), linearToSrgb(c[1]), linearToSrgb(c[2]), 1);
            this.bursts.push(this.directions.map(() => this.collection.add({ position: Cartesian3.ZERO, image, color: colour, show: false, sizeInMeters: true, width: 0, height: 0 })));
        });
        this.playSeconds = Math.max(1, (f.burstOffsets.length - 1) * f.burstInterval + f.burstSeconds);
    }

    protected override onPlayStarted(): void {
        const f = this.data.fireworks!;
        this.playSeconds = Math.max(1, (f.burstOffsets.length - 1) * f.burstInterval + f.burstSeconds);
    }

    protected override onStopped(): void {
        for (const burst of this.bursts) for (const b of burst) b.show = false;
    }

    protected override applyAlpha(_alpha: number, elapsedSeconds: number): void {
        // The actor itself never moves; each burst is drawn from its own clock.
        if (!this.isPlaying()) return;
        const f = this.data.fireworks!;
        f.burstOffsets.forEach((offset, index) => {
            const t = (elapsedSeconds - index * f.burstInterval) / f.burstSeconds;
            const visible = t > 0 && t < 1;
            const burst = this.bursts[index];
            if (!visible) {
                for (const b of burst) b.show = false;
                return;
            }
            // Opens quickly then slows (ease-out), drifts down, and every spark shrinks to nothing in the last half.
            const open = 1 - Math.pow(1 - clamp(t / 0.55, 0, 1), 3);
            const shrink = 1 - smoothStep(0.45, 1, t);
            const grow = smoothStep(0, 0.12, t);
            const size = (f.sparkSize / SPHERE_DIAMETER) * shrink * grow; // metres (the 100 cm sphere scaled)
            const drop = SPARK_DROP * t * t;
            const centre = this.rest.transformPositionNoScale(new Vec3(offset[0], offset[1], offset[2]));
            this.directions.forEach((d, i) => {
                const p = centre.add(d.scale(f.burstRadius * open)).sub(new Vec3(0, 0, drop));
                const b = burst[i];
                b.position = this.geo.worldToFixed(p, b.position ?? new Cartesian3());
                b.width = b.height = size;
                b.show = true;
            });
        });
    }

    protected override setCueHidden(hidden: boolean): void {
        this.hidden = hidden;
        if (hidden) this.onStopped();
    }
}
