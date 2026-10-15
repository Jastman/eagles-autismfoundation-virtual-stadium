// The level's stand-in props as CesiumJS primitives: Unreal's basic shapes (cubes, cylinders, spheres, planes) with
// their material colours, imported props drawn as their mesh bounds, City Sample cars as car-sized bodies and MetaHumans
// as simple figures (both are Unreal-only content). Static props share a few batched primitives; anything that moves or
// hides on its own (cues, labels, crowd, the parking lot) gets its own, moved through its model matrix.
import {
    BoxGeometry, Color, ColorGeometryInstanceAttribute, CylinderGeometry, EllipsoidGeometry, GeometryInstance, Matrix4, PerInstanceColorAppearance,
    Primitive, Cartesian3, ShadowMode,
} from "cesium";
import type { Scene } from "cesium";
import { Quat, Rotator, Transform, Vec3 } from "../util/ue";
import { ueTransformToLocalMatrix } from "./geo";
import type { Geo } from "./geo";
import type { ActorData, Part, XF } from "./sceneData";
import { linearToSrgb, toTransform } from "./sceneData";
import { carInstances } from "./vehicles";

const BOX = BoxGeometry.fromDimensions({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, dimensions: new Cartesian3(100, 100, 100) });
const CYLINDER = new CylinderGeometry({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, length: 100, topRadius: 50, bottomRadius: 50, slices: 24 });
const CONE = new CylinderGeometry({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, length: 100, topRadius: 0, bottomRadius: 50, slices: 24 });
const SPHERE = new EllipsoidGeometry({ vertexFormat: PerInstanceColorAppearance.VERTEX_FORMAT, radii: new Cartesian3(50, 50, 50), stackPartitions: 16, slicePartitions: 24 });

/** Deterministic 0..1 from a string (same figure colours on every visit). */
export function hash01(text: string, salt = 0): number {
    let h = 2166136261 ^ salt;
    for (let i = 0; i < text.length; ++i) h = Math.imul(h ^ text.charCodeAt(i), 16777619);
    return ((h >>> 0) % 100000) / 100000;
}

const SHIRTS = [[0.0, 0.3, 0.33], [0.95, 0.95, 0.93], [0.08, 0.08, 0.09], [0.41, 0.75, 0.16], [0.65, 0.66, 0.68], [0.0, 0.3, 0.33]];
const SKINS = [[0.94, 0.78, 0.66], [0.78, 0.57, 0.42], [0.55, 0.38, 0.26], [0.36, 0.24, 0.16], [0.86, 0.68, 0.53]];
const TROUSERS = [[0.12, 0.14, 0.2], [0.2, 0.2, 0.21], [0.3, 0.27, 0.22], [0.1, 0.1, 0.11]];

const colourOf = (c: number[], alpha = 1): Color => new Color(linearToSrgb(c[0]), linearToSrgb(c[1]), linearToSrgb(c[2]), alpha);
const srgb = (c: number[]): Color => new Color(c[0], c[1], c[2], 1);

function instance(geometry: BoxGeometry | CylinderGeometry | EllipsoidGeometry, matrix: Matrix4, colour: Color): GeometryInstance {
    return new GeometryInstance({ geometry, modelMatrix: matrix, attributes: { color: ColorGeometryInstanceAttribute.fromColor(colour) } });
}

/** A local Unreal transform (relative to the actor root, cm) composed with a shape-local offset/scale. */
function local(xf: Transform, offset = new Vec3(), scale = new Vec3(1, 1, 1), rotation = new Quat()): Matrix4 {
    const inner = new Transform(rotation, offset, scale);
    return ueTransformToLocalMatrix(inner.mul(xf));
}

const HAIR = [[0.13, 0.1, 0.08], [0.27, 0.19, 0.12], [0.47, 0.35, 0.23], [0.74, 0.62, 0.43], [0.06, 0.05, 0.05], [0.24, 0.24, 0.24]];
const SHOES = [[0.08, 0.08, 0.09], [0.92, 0.92, 0.9], [0.32, 0.22, 0.14]];

/**
 * A person in the actor's frame (feet at its origin, facing +X): shoes, legs, hips, torso, arms, hands, neck, head and a
 * hair shape for the MetaHuman's hairstyle. Build follows the MetaHuman's body (female or male); colours vary per person.
 */
function personInstances(key: string, at: Transform, look: Part["look"]): GeometryInstance[] {
    const pick = <T>(list: T[], salt: number) => list[Math.floor(hash01(key, salt) * list.length)];
    const shirt = srgb(pick(SHIRTS, 1));
    const skin = srgb(pick(SKINS, 2));
    const trousers = srgb(pick(TROUSERS, 3));
    const hair = srgb(pick(HAIR, 5));
    const shoes = srgb(pick(SHOES, 6));
    const female = look?.sex === "f";
    const k = (female ? 0.94 : 1) * (0.95 + hash01(key, 4) * 0.1);
    const shoulders = female ? 19 : 22;
    const up = new Quat();
    const part = (geometry: typeof BOX | typeof CYLINDER | typeof SPHERE, x: number, y: number, z: number, sx: number, sy: number, sz: number, colour: Color) =>
        instance(geometry, local(at, new Vec3(x, y, z * k), new Vec3(sx, sy, sz * k), up), colour);
    const out = [
        part(BOX, 4, 9, 4, 0.26, 0.1, 0.08, shoes),
        part(BOX, 4, -9, 4, 0.26, 0.1, 0.08, shoes),
        part(CYLINDER, 0, 9, 48, 0.15, 0.15, 0.82, trousers),
        part(CYLINDER, 0, -9, 48, 0.15, 0.15, 0.82, trousers),
        part(SPHERE, 0, 0, 92, 0.26, female ? 0.38 : 0.36, 0.2, trousers),
        part(CYLINDER, 0, 0, 120, 0.24, shoulders * 0.019, 0.5, shirt),
        part(SPHERE, 0, 0, 142, 0.26, shoulders * 0.02, 0.14, shirt),
        part(CYLINDER, 0, shoulders + 4, 115, 0.1, 0.1, 0.56, shirt),
        part(CYLINDER, 0, -(shoulders + 4), 115, 0.1, 0.1, 0.56, shirt),
        part(SPHERE, 1, shoulders + 4, 84, 0.1, 0.08, 0.12, skin),
        part(SPHERE, 1, -(shoulders + 4), 84, 0.1, 0.08, 0.12, skin),
        part(CYLINDER, 0, 0, 150, 0.1, 0.1, 0.12, skin),
        part(SPHERE, 1, 0, 163, 0.2, 0.17, 0.24, skin),
    ];
    // Hair, after the MetaHuman's groom (S_ short, M_ medium).
    const style = look?.hair ?? (female ? "M_BobStraight" : "S_Clean");
    if (style.startsWith("M_")) {
        out.push(part(SPHERE, -3, 0, 165, 0.25, 0.21, 0.27, hair));
        out.push(part(BOX, -6, 0, 154, 0.16, 0.21, 0.16, hair));
    } else if (style.includes("PulledBack")) {
        out.push(part(SPHERE, -2, 0, 168, 0.22, 0.185, 0.2, hair));
        out.push(part(SPHERE, -13, 0, 166, 0.08, 0.08, 0.08, hair));
    } else if (style.includes("Curly")) {
        out.push(part(SPHERE, -1, 0, 171, 0.215, 0.185, 0.16, hair));
    } else {
        out.push(part(SPHERE, -1.5, 0, 169, 0.21, 0.18, 0.16, hair));
    }
    return out;
}

/** Geometry instances for one part (and each of its instances) in the actor's root frame. */
export function partInstances(actor: ActorData, part: Part, index: number): GeometryInstance[] {
    const transforms: XF[] = part.instances ?? (part.l ? [part as XF] : []);
    const out: GeometryInstance[] = [];
    transforms.forEach((xf, i) => {
        const at = toTransform(xf);
        const key = `${actor.label}/${index}/${i}`;
        switch (part.shape) {
            case "cube":
                out.push(instance(BOX, local(at), colourOf(part.colour)));
                break;
            case "cylinder":
                out.push(instance(CYLINDER, local(at), colourOf(part.colour)));
                break;
            case "cone":
                out.push(instance(CONE, local(at), colourOf(part.colour)));
                break;
            case "sphere":
                out.push(instance(SPHERE, local(at), colourOf(part.colour)));
                break;
            case "plane":
                out.push(instance(BOX, local(at, new Vec3(), new Vec3(1, 1, 0.01)), colourOf(part.colour)));
                break;
            case "box": {
                const b = part.bounds ?? { o: [0, 0, 50], e: [50, 50, 50] };
                out.push(instance(BOX, local(at, new Vec3(b.o[0], b.o[1], b.o[2]), new Vec3(b.e[0] / 50, b.e[1] / 50, b.e[2] / 50)), colourOf(part.colour)));
                break;
            }
            case "car":
                out.push(...carInstances(part.vehicle ?? "02", at, colourOf(part.colour)));
                break;
            case "person":
                // MetaHuman body meshes face +Y and are turned -90 to face the actor's +X; undo that so the figure (drawn
                // facing +X, shoulders across Y) faces the same way.
                out.push(...personInstances(key, new Transform(at.rotation.mul(new Rotator(0, 90, 0).quaternion()).normalized(), at.translation), part.look));
                break;
        }
    });
    return out;
}

export class PropPrimitive {
    readonly primitive: Primitive;
    private shown = true;

    constructor(private readonly scene: Scene, instances: GeometryInstance[], modelMatrix: Matrix4) {
        this.primitive = new Primitive({
            geometryInstances: instances,
            appearance: new PerInstanceColorAppearance({ flat: false, translucent: false, closed: true }),
            modelMatrix,
            // Synchronous: the cars and people are prebuilt geometry, which the worker path can't take.
            asynchronous: false,
            allowPicking: false,
            releaseGeometryInstances: true,
            compressVertices: true,
            shadows: ShadowMode.ENABLED,
        });
        scene.primitives.add(this.primitive);
    }

    setModelMatrix(matrix: Matrix4): void {
        Matrix4.clone(matrix, this.primitive.modelMatrix);
    }

    setShown(shown: boolean): void {
        if (this.shown === shown) return;
        this.shown = shown;
        this.primitive.show = shown;
    }

    isShown(): boolean {
        return this.shown;
    }

    destroy(): void {
        this.scene.primitives.remove(this.primitive);
    }
}

/** The actor's root world transform; its parts are drawn relative to it, root scale included in the parts. */
export function rootTransform(actor: ActorData): Transform {
    const root = toTransform(actor.root);
    // Parts carry the root's scale already (tools/build_scene_data.mjs), so the frame is unscaled.
    return new Transform(root.rotation, root.translation, new Vec3(1, 1, 1));
}

export function modelMatrixFor(geo: Geo, world: Transform, result = new Matrix4()): Matrix4 {
    return geo.worldTransformToModelMatrix(world, result);
}
