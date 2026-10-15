// The level's stand-in props as CesiumJS primitives: Unreal's basic shapes (cubes, cylinders, spheres, planes) with
// their material colours, imported props drawn as their mesh bounds, City Sample cars as car-sized bodies and MetaHumans
// as simple figures (both are Unreal-only content). Static props share a few batched primitives; anything that moves or
// hides on its own (cues, labels, crowd, the parking lot) gets its own, moved through its model matrix.
import {
    BoxGeometry, Color, ColorGeometryInstanceAttribute, CylinderGeometry, EllipsoidGeometry, GeometryInstance, Matrix4, PerInstanceColorAppearance,
    Primitive, Cartesian3,
} from "cesium";
import type { Scene } from "cesium";
import { Quat, Rotator, Transform, Vec3 } from "../util/ue";
import { ueTransformToLocalMatrix } from "./geo";
import type { Geo } from "./geo";
import type { ActorData, Part, XF } from "./sceneData";
import { linearToSrgb, toTransform } from "./sceneData";

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

/** A person: legs, torso, head, in the actor's frame (feet at its origin, facing +X). Colours vary per person. */
function personInstances(key: string, at: Transform): GeometryInstance[] {
    const shirt = srgb(SHIRTS[Math.floor(hash01(key, 1) * SHIRTS.length)]);
    const skin = srgb(SKINS[Math.floor(hash01(key, 2) * SKINS.length)]);
    const trousers = srgb(TROUSERS[Math.floor(hash01(key, 3) * TROUSERS.length)]);
    const tall = 0.92 + hash01(key, 4) * 0.14;
    const upright = new Quat();
    return [
        instance(CYLINDER, local(at, new Vec3(0, 0, 44 * tall), new Vec3(0.34, 0.42, 0.88 * tall), upright), trousers),
        instance(CYLINDER, local(at, new Vec3(0, 0, 118 * tall), new Vec3(0.36, 0.48, 0.62 * tall), upright), shirt),
        instance(SPHERE, local(at, new Vec3(0, 0, 162 * tall), new Vec3(0.21, 0.19, 0.25), upright), skin),
    ];
}

/** A City Sample car body: about 4.6 x 1.9 x 1.45 m, wheels and glass included in the shape's colour blocks. */
function carInstances(at: Transform, paint: Color): GeometryInstance[] {
    const glass = Color.fromBytes(30, 36, 42);
    const tyre = Color.fromBytes(18, 18, 20);
    return [
        instance(BOX, local(at, new Vec3(0, 0, 58), new Vec3(4.55, 1.86, 0.72)), paint),
        instance(BOX, local(at, new Vec3(-25, 0, 112), new Vec3(2.6, 1.7, 0.5)), glass),
        instance(BOX, local(at, new Vec3(135, 0, 34), new Vec3(0.7, 1.9, 0.68)), tyre),
        instance(BOX, local(at, new Vec3(-140, 0, 34), new Vec3(0.7, 1.9, 0.68)), tyre),
    ];
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
                out.push(...carInstances(at, colourOf(part.colour)));
                break;
            case "person":
                // MetaHuman body meshes face +Y and are turned -90 to face the actor's +X; undo that so the figure (drawn
                // facing +X, shoulders across Y) faces the same way.
                out.push(...personInstances(key, new Transform(at.rotation.mul(new Rotator(0, 90, 0).quaternion()).normalized(), at.translation)));
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
            asynchronous: true,
            allowPicking: false,
            releaseGeometryInstances: true,
            compressVertices: true,
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
