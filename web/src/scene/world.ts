// L_LFF_Stadium on the CesiumJS globe: the camera targets and routes, the stand-in props, signs, scene cues, crowd and
// the interior/quiet-room volumes, behind the JourneyWorld interface the ported journey reads (journey/world.ts).
import { Matrix4 } from "cesium";
import type { Scene } from "cesium";
import type { CameraPath, CameraTarget, CueActor, JourneyWorld, QuietZone, WorldLabel } from "../journey/world";
import { Transform, Vec3 } from "../util/ue";
import { FireworksCue, LevelCue, PrimitiveVisual } from "./cues";
import { StadiumCrowd } from "./crowd";
import { EagleVisual } from "./eagle";
import { CameraEffects } from "./effects";
import type { Geo } from "./geo";
import { WorldText } from "./labels";
import { PropPrimitive, partInstances, rootTransform } from "./props";
import type { ActorData, SceneData } from "./sceneData";
import { targetTransform, toTransform, vec } from "./sceneData";

interface Obb {
    /** World frame of the box (unscaled), with its half extents in cm. */
    frame: Transform;
    half: Vec3;
}

export class StadiumWorld implements JourneyWorld {
    readonly siteFrame: Transform;
    readonly cameraPaths: CameraPath[];
    readonly cueActors: LevelCue[] = [];
    readonly labels: WorldLabel[] = [];
    readonly quietZones: QuietZone[] = [];
    private readonly targets = new Map<string, CameraTarget>();
    private readonly crowdProps: PropPrimitive[] = [];
    private readonly exteriorProps: PropPrimitive[] = [];
    private readonly interiorBoxes: Obb[] = [];
    private readonly obstacles: Obb[] = [];
    private crowd: StadiumCrowd | null = null;
    private crowdShown = true;
    readonly effects: CameraEffects;

    constructor(private readonly scene: Scene, private readonly geo: Geo, private readonly data: SceneData, models: Record<string, string>) {
        this.siteFrame = geo.siteFrame;
        for (const [id, t] of Object.entries(data.targets)) this.targets.set(id, { id, transform: targetTransform(t), fov: t.fov });
        this.cameraPaths = data.paths.map((p) => ({ name: p.name, fromTargetId: p.from, toTargetId: p.to, travelSeconds: p.travelSeconds,
            cutAtEnd: p.cutAtEnd, waypoints: p.waypoints.map(vec) }));
        this.effects = new CameraEffects(scene, geo);
        this.buildActors(models);
    }

    private buildActors(models: Record<string, string>): void {
        const byLabel = new Map(this.data.actors.map((a) => [a.label, a]));
        const children = new Map<string, ActorData[]>();
        for (const actor of this.data.actors) {
            if (actor.attachedTo && byLabel.get(actor.attachedTo)?.cue) {
                children.set(actor.attachedTo, [...(children.get(actor.attachedTo) ?? []), actor]);
            }
        }
        const staticInstances: ReturnType<typeof partInstances> = [];
        const enu = this.geo.enuToFixed;

        for (const actor of this.data.actors) {
            const root = rootTransform(actor);
            const instances = actor.parts.filter((p) => !p.hidden).flatMap((p, i) => partInstances(actor, p, i));
            for (const box of actor.boxes ?? []) this.addVolume(actor, box);
            if (!actor.cue) {
                for (const p of actor.parts) if (!p.hidden && p.shape !== "person") this.addObstacles(actor, p, root);
            }

            if (actor.text) {
                const text = new WorldText(this.scene, actor, root);
                text.setModelMatrix(enu);
                const plate = instances.length > 0 ? new PropPrimitive(this.scene, instances, this.geo.worldTransformToModelMatrix(root)) : null;
                if (actor.tags.includes("LFF_Label")) {
                    const tags = actor.tags;
                    this.labels.push({ tags, location: text.location, setHidden: (hidden) => {
                        text.setHidden(hidden);
                        plate?.setShown(!hidden);
                    } });
                    // Labels start hidden; the journey shows the step's own.
                    text.setHidden(true);
                    plate?.setShown(false);
                }
                continue;
            }

            if (actor.cue) {
                const rest = toTransform(actor.root);
                const cue = actor.cue.fireworks ? new FireworksCue(actor.label, actor.cue, rest, this.geo, this.scene) : new LevelCue(actor.label, actor.cue, rest);
                if (instances.length > 0) cue.visuals.push(new PrimitiveVisual(this.geo, new PropPrimitive(this.scene, instances, Matrix4.clone(Matrix4.IDENTITY))));
                for (const child of children.get(actor.label) ?? []) {
                    // An attached actor rides its parent cue at its rest offset (the eagle's mesh on its flight).
                    const relative = toTransform(child.root).mul(new Transform(rest.rotation.inverse(), rest.rotation.inverse().rotateVector(rest.translation.scale(-1))));
                    if (child.eagle) cue.visuals.push(new EagleVisual(this.geo, this.scene, relative, models["eagle"] ?? null));
                    const childInstances = child.parts.filter((p) => !p.hidden).flatMap((p, i) => partInstances(child, p, i));
                    if (childInstances.length > 0) {
                        const prop = new PropPrimitive(this.scene, childInstances, Matrix4.clone(Matrix4.IDENTITY));
                        cue.visuals.push({
                            setWorld: (w) => prop.setModelMatrix(this.geo.worldTransformToModelMatrix(relative.mul(w))),
                            setShown: (s) => prop.setShown(s),
                        });
                    }
                }
                this.cueActors.push(cue);
                cue.applyState("Before");
                continue;
            }
            if (actor.attachedTo && byLabel.get(actor.attachedTo)?.cue) continue;
            if (instances.length === 0) continue;

            const isCrowd = actor.tags.includes("LFF_Crowd");
            const isExterior = actor.tags.includes("LFF_Exterior");
            if (isCrowd || isExterior) {
                const prop = new PropPrimitive(this.scene, instances, this.geo.worldTransformToModelMatrix(root));
                (isCrowd ? this.crowdProps : this.exteriorProps).push(prop);
            } else {
                // Static: batched in one primitive with each part's full world matrix.
                const rootMatrix = this.geo.worldTransformToModelMatrix(root);
                for (const instance of instances) {
                    instance.modelMatrix = Matrix4.multiply(rootMatrix, instance.modelMatrix, new Matrix4());
                    staticInstances.push(instance);
                }
            }
        }
        if (staticInstances.length > 0) new PropPrimitive(this.scene, staticInstances, Matrix4.clone(Matrix4.IDENTITY));
        console.info(`Level: ${this.data.actors.length} actors, ${this.cueActors.length} cues, ${this.labels.length} labels, ` +
            `${this.crowdProps.length} crowd and ${this.exteriorProps.length} exterior groups, ${this.obstacles.length} obstacles.`);
    }

    private addVolume(actor: ActorData, box: { extent: number[]; world: { l: number[]; q: number[]; s: number[] } }): void {
        const world = toTransform(box.world);
        const half = new Vec3(box.extent[0] * world.scale3D.x, box.extent[1] * world.scale3D.y, box.extent[2] * world.scale3D.z);
        const obb = { frame: new Transform(world.rotation, world.translation), half };
        if (actor.tags.includes("LFF_StadiumInterior")) this.interiorBoxes.push(obb);
        const sensory = actor.tags.includes("LFF_SensoryAudioZone");
        if (sensory || actor.tags.includes("LFF_QuietAudioZone")) {
            this.quietZones.push({ kind: sensory ? "Sensory" : "Quiet", blend: 80, distanceTo: (p) => obbDistance(obb, p) });
        }
    }

    /** Collision for the camera's wall check: the stand-ins' shapes (Unreal's stadium model has no collision either). */
    private addObstacles(actor: ActorData, part: ActorData["parts"][number], root: Transform): void {
        if (actor.class === "TextRenderActor") return;
        const transforms = part.instances ?? (part.l ? [part as { l: number[]; q: number[]; s: number[] }] : []);
        for (const xf of transforms) {
            const local = toTransform(xf);
            let centre = new Vec3();
            let half = new Vec3(50, 50, 50);
            if (part.shape === "box" && part.bounds) {
                centre = vec(part.bounds.o);
                half = vec(part.bounds.e);
            } else if (part.shape === "car") {
                centre = new Vec3(0, 0, 72);
                half = new Vec3(230, 95, 72);
            } else if (part.shape === "plane") {
                half = new Vec3(50, 50, 1);
            }
            const world = local.mul(root);
            this.obstacles.push({
                frame: new Transform(world.rotation, world.transformPosition(centre)),
                half: half.mul(new Vec3(Math.abs(world.scale3D.x), Math.abs(world.scale3D.y), Math.abs(world.scale3D.z))),
            });
        }
    }

    async loadCrowd(): Promise<void> {
        const seat = this.targets.get("CT_SeatFieldView")?.transform.translation ?? null;
        this.crowd = await StadiumCrowd.load(this.scene, this.geo, seat);
        this.crowd.setShown(this.crowdShown);
    }

    findTarget(id: string): CameraTarget | undefined {
        return this.targets.get(id);
    }

    placeActorLocation(tag: string): Vec3 | undefined {
        const p = this.data.places[tag];
        return p ? vec(p) : undefined;
    }

    setCrowd(shown: boolean, _animate: boolean): void {
        this.crowdShown = shown;
        for (const prop of this.crowdProps) prop.setShown(shown);
        this.crowd?.setShown(shown);
    }

    isInsideStadium(point: Vec3): boolean {
        return this.interiorBoxes.some((box) => obbDistance(box, point) <= 0);
    }

    setExteriorHidden(hidden: boolean): void {
        for (const prop of this.exteriorProps) prop.setShown(!hidden);
    }

    isPathBlocked(from: Vec3, to: Vec3, radius: number): boolean {
        if (from.equals(to, 1)) return false;
        return this.obstacles.some((box) => segmentHitsObb(box, from, to, radius));
    }

    setLevelHighlight(band: { minZ: number; maxZ: number } | null): void {
        this.effects.setLevelHighlight(band);
    }

    viewportAspect(): number {
        const canvas = this.scene.canvas;
        return canvas.clientHeight > 0 ? canvas.clientWidth / canvas.clientHeight : 16 / 9;
    }

    tick(dt: number): void {
        for (const cue of this.cueActors) cue.tick(dt);
    }

    cues(): CueActor[] {
        return this.cueActors;
    }
}

/** Distance from a box (0 inside). */
function obbDistance(box: Obb, p: Vec3): number {
    const local = box.frame.inverseTransformPositionNoScale(p);
    const dx = Math.max(Math.abs(local.x) - box.half.x, 0);
    const dy = Math.max(Math.abs(local.y) - box.half.y, 0);
    const dz = Math.max(Math.abs(local.z) - box.half.z, 0);
    return Math.hypot(dx, dy, dz);
}

/** A sphere swept along a segment against a box, approximated by the box grown by the radius (slab test). */
function segmentHitsObb(box: Obb, from: Vec3, to: Vec3, radius: number): boolean {
    const a = box.frame.inverseTransformPositionNoScale(from);
    const b = box.frame.inverseTransformPositionNoScale(to);
    const d = b.sub(a);
    let t0 = 0, t1 = 1;
    for (const axis of ["x", "y", "z"] as const) {
        const half = box.half[axis] + radius;
        if (Math.abs(d[axis]) < 1e-9) {
            if (Math.abs(a[axis]) > half) return false;
            continue;
        }
        let near = (-half - a[axis]) / d[axis];
        let far = (half - a[axis]) / d[axis];
        if (near > far) [near, far] = [far, near];
        t0 = Math.max(t0, near);
        t1 = Math.min(t1, far);
        if (t0 > t1) return false;
    }
    return true;
}

