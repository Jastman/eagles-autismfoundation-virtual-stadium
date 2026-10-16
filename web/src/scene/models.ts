// Meshes exported from Unreal (Tools/Editor/export_web_assets.py -> web/assets-src/models/<AssetName>.glb, with
// <AssetName>.bounds.json holding the mesh's Unreal-local bounds). Where a part's Unreal mesh has been exported, the
// model replaces the stand-in, at the same transforms.
//
// Orientation: the glTF exporter changes axes and units; rather than trust one convention, each GLB's own bounds are
// matched against the Unreal bounds to find the axis mapping (a signed permutation; Unreal is left-handed, so the
// mapping is expected to be a reflection) and the unit scale.
//
// Meshes repeated many times (the hundreds of lot cars) are drawn as one instanced model each: the GLB is rewritten
// in the browser with EXT_mesh_gpu_instancing on its mesh nodes.
import { Axis, Cartesian3, Matrix3, Matrix4, Model, ModelAnimationLoop, ShadowMode } from "cesium";
import type { Scene } from "cesium";
import { Transform, Vec3 } from "../util/ue";
import type { Geo } from "./geo";

export interface UnrealBounds {
    min: number[];
    max: number[];
}

interface GltfJson {
    asset: unknown;
    scene?: number;
    scenes?: { nodes?: number[] }[];
    nodes?: { mesh?: number; skin?: number; children?: number[]; matrix?: number[]; translation?: number[]; rotation?: number[]; scale?: number[]; extensions?: Record<string, unknown> }[];
    meshes?: { primitives: { attributes: Record<string, number> }[] }[];
    accessors?: { bufferView?: number; byteOffset?: number; componentType: number; count: number; type: string; min?: number[]; max?: number[] }[];
    bufferViews?: { buffer: number; byteOffset?: number; byteLength: number; byteStride?: number; target?: number }[];
    buffers?: { byteLength: number; uri?: string }[];
    animations?: unknown[];
    extensionsUsed?: string[];
}

export interface Glb {
    json: GltfJson;
    bin: Uint8Array;
}

const GLB_MAGIC = 0x46546c67, CHUNK_JSON = 0x4e4f534a, CHUNK_BIN = 0x004e4942;

export function parseGlb(data: ArrayBuffer): Glb {
    const view = new DataView(data);
    if (view.getUint32(0, true) !== GLB_MAGIC) throw new Error("not a GLB");
    let offset = 12;
    let json: GltfJson | null = null;
    let bin = new Uint8Array(0);
    while (offset < view.byteLength) {
        const length = view.getUint32(offset, true), type = view.getUint32(offset + 4, true);
        const chunk = new Uint8Array(data, offset + 8, length);
        if (type === CHUNK_JSON) json = JSON.parse(new TextDecoder().decode(chunk)) as GltfJson;
        else if (type === CHUNK_BIN) bin = chunk.slice();
        offset += 8 + length;
    }
    if (!json) throw new Error("GLB without JSON");
    return { json, bin };
}

export function writeGlb(glb: Glb): Uint8Array {
    const pad = (n: number) => (4 - (n % 4)) % 4;
    let jsonBytes = new TextEncoder().encode(JSON.stringify(glb.json));
    jsonBytes = Uint8Array.from([...jsonBytes, ...new Array<number>(pad(jsonBytes.length)).fill(0x20)]);
    const binLength = glb.bin.length + pad(glb.bin.length);
    const total = 12 + 8 + jsonBytes.length + (glb.bin.length > 0 ? 8 + binLength : 0);
    const out = new Uint8Array(total);
    const view = new DataView(out.buffer);
    view.setUint32(0, GLB_MAGIC, true);
    view.setUint32(4, 2, true);
    view.setUint32(8, total, true);
    view.setUint32(12, jsonBytes.length, true);
    view.setUint32(16, CHUNK_JSON, true);
    out.set(jsonBytes, 20);
    if (glb.bin.length > 0) {
        const at = 20 + jsonBytes.length;
        view.setUint32(at, binLength, true);
        view.setUint32(at + 4, CHUNK_BIN, true);
        out.set(glb.bin, at + 8);
    }
    return out;
}

function nodeLocal(node: NonNullable<GltfJson["nodes"]>[number]): Matrix4 {
    if (node.matrix) return Matrix4.fromArray(node.matrix);
    const t = node.translation ?? [0, 0, 0], r = node.rotation ?? [0, 0, 0, 1], s = node.scale ?? [1, 1, 1];
    // Column-major TRS.
    const [x, y, z, w] = r;
    const m = Matrix3.fromArray([
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y),
    ]);
    const scaled = Matrix3.multiplyByScale(m, new Cartesian3(s[0], s[1], s[2]), new Matrix3());
    return Matrix4.fromRotationTranslation(scaled, new Cartesian3(t[0], t[1], t[2]));
}

/** Every mesh node with its global matrix (scene roots down). */
export function meshNodes(json: GltfJson): { node: number; global: Matrix4 }[] {
    const out: { node: number; global: Matrix4 }[] = [];
    const nodes = json.nodes ?? [];
    const visit = (index: number, parent: Matrix4) => {
        const global = Matrix4.multiply(parent, nodeLocal(nodes[index]), new Matrix4());
        if (nodes[index].mesh !== undefined) out.push({ node: index, global });
        for (const child of nodes[index].children ?? []) visit(child, global);
    };
    const roots = json.scenes?.[json.scene ?? 0]?.nodes ?? nodes.map((_, i) => i);
    for (const root of roots) visit(root, Matrix4.IDENTITY);
    return out;
}

/** The model's bounds in its own (glTF) space, from the POSITION accessors' min/max. */
export function glbBounds(json: GltfJson): UnrealBounds | null {
    const min = [Infinity, Infinity, Infinity], max = [-Infinity, -Infinity, -Infinity];
    for (const { node, global } of meshNodes(json)) {
        for (const primitive of json.meshes?.[json.nodes![node].mesh!]?.primitives ?? []) {
            const accessor = json.accessors?.[primitive.attributes.POSITION];
            if (!accessor?.min || !accessor.max) continue;
            for (let corner = 0; corner < 8; ++corner) {
                const p = [0, 1, 2].map((k) => ((corner >> k) & 1 ? accessor.max![k] : accessor.min![k]));
                const g = Matrix4.multiplyByPoint(global, new Cartesian3(p[0], p[1], p[2]), new Cartesian3());
                [g.x, g.y, g.z].forEach((v, k) => {
                    min[k] = Math.min(min[k], v);
                    max[k] = Math.max(max[k], v);
                });
            }
        }
    }
    return Number.isFinite(min[0]) ? { min, max } : null;
}

const PERMUTATIONS = [[0, 1, 2], [0, 2, 1], [1, 0, 2], [1, 2, 0], [2, 0, 1], [2, 1, 0]];

/**
 * The matrix from glTF model space to the mesh's Unreal-local space (cm): a signed axis permutation times a unit scale,
 * chosen so the GLB's bounds land on the Unreal bounds. Reflections (what a correct left-to-right-handed export needs)
 * are preferred unless a rotation fits clearly better.
 */
export function fitAxes(model: UnrealBounds, unreal: UnrealBounds): { matrix: Matrix4; error: number; reflection: boolean } {
    const size = (b: UnrealBounds) => [0, 1, 2].map((k) => b.max[k] - b.min[k]).sort((a, c) => a - c);
    const ms = size(model), us = size(unreal);
    // The unit scale: the exact ratio of the longest sides (an import may scale), snapped to a power of ten when it is
    // within 3% of one (a plain metres-to-centimetres export).
    const ratio = (us[2] || 1) / (ms[2] || 1);
    const decade = Math.pow(10, Math.round(Math.log10(ratio)));
    const scale = Math.abs(ratio / decade - 1) < 0.03 ? decade : ratio;
    let best = { matrix: Matrix4.IDENTITY, error: Infinity, reflection: false };
    let bestProper = { ...best };
    for (const perm of PERMUTATIONS) {
        const parity = [[0, 1, 2], [1, 2, 0], [2, 0, 1]].some((p) => p.join() === perm.join()) ? 1 : -1;
        for (let signs = 0; signs < 8; ++signs) {
            const sign = [0, 1, 2].map((k) => ((signs >> k) & 1 ? -1 : 1));
            // Unreal axis k takes model axis perm[k] times sign[k].
            let error = 0;
            for (let k = 0; k < 3; ++k) {
                const a = model.min[perm[k]] * sign[k] * scale, b = model.max[perm[k]] * sign[k] * scale;
                error += Math.abs(Math.min(a, b) - unreal.min[k]) + Math.abs(Math.max(a, b) - unreal.max[k]);
            }
            const det = parity * sign[0] * sign[1] * sign[2];
            const m = new Array<number>(16).fill(0);
            for (let k = 0; k < 3; ++k) m[perm[k] * 4 + k] = sign[k] * scale; // column perm[k], row k (column-major)
            m[15] = 1;
            const candidate = { matrix: Matrix4.fromArray(m), error, reflection: det < 0 };
            if (det < 0 && error < best.error) best = candidate;
            if (det > 0 && error < bestProper.error) bestProper = candidate;
        }
    }
    return bestProper.error < best.error * 0.5 ? bestProper : best;
}

/** Adds EXT_mesh_gpu_instancing to every mesh node: instance i is placed by `instances[i]` (in model space, before the node). */
export function instanceGlb(glb: Glb, instances: Matrix4[]): Glb {
    const json: GltfJson = JSON.parse(JSON.stringify(glb.json));
    const count = instances.length;
    const nodes = meshNodes(json).filter(({ node }) => json.nodes![node].skin === undefined);
    const chunks: Uint8Array[] = [glb.bin];
    let length = glb.bin.length;
    const append = (values: Float32Array, type: "VEC3" | "VEC4") => {
        length += (4 - (length % 4)) % 4;
        const padded = chunks.reduce((n, c) => n + c.length, 0);
        if (padded < length) chunks.push(new Uint8Array(length - padded));
        const bytes = new Uint8Array(values.buffer.slice(0));
        json.bufferViews = json.bufferViews ?? [];
        json.bufferViews.push({ buffer: 0, byteOffset: length, byteLength: bytes.length });
        json.accessors = json.accessors ?? [];
        json.accessors.push({ bufferView: json.bufferViews.length - 1, componentType: 5126, count, type });
        chunks.push(bytes);
        length += bytes.length;
        return json.accessors.length - 1;
    };
    for (const { node, global } of nodes) {
        const inverse = Matrix4.inverse(global, new Matrix4());
        const translation = new Float32Array(count * 3), rotation = new Float32Array(count * 4), scale = new Float32Array(count * 3);
        instances.forEach((instance, i) => {
            // Node-local instance transform, so the node's own transform still applies after it.
            const local = Matrix4.multiply(inverse, Matrix4.multiply(instance, global, new Matrix4()), new Matrix4());
            const trs = decompose(local);
            translation.set(trs.t, i * 3);
            rotation.set(trs.r, i * 4);
            scale.set(trs.s, i * 3);
        });
        json.nodes![node].extensions = {
            ...(json.nodes![node].extensions ?? {}),
            EXT_mesh_gpu_instancing: { attributes: { TRANSLATION: append(translation, "VEC3"), ROTATION: append(rotation, "VEC4"), SCALE: append(scale, "VEC3") } },
        };
    }
    json.extensionsUsed = [...new Set([...(json.extensionsUsed ?? []), "EXT_mesh_gpu_instancing"])];
    const bin = new Uint8Array(length);
    let at = 0;
    for (const c of chunks) {
        bin.set(c, at);
        at += c.length;
    }
    // The instance data went onto the end of the GLB's own buffer (buffer 0); any other buffers are untouched.
    json.buffers = json.buffers && json.buffers.length > 0 ? json.buffers.map((b, i) => (i === 0 ? { ...b, byteLength: bin.length } : b))
        : [{ byteLength: bin.length }];
    return { json, bin };
}

/** TRS of a proper similarity (rotation and positive scale). */
function decompose(m: Matrix4): { t: number[]; r: number[]; s: number[] } {
    const t = [m[12], m[13], m[14]];
    const col = (c: number) => [m[c * 4], m[c * 4 + 1], m[c * 4 + 2]];
    const s = [0, 1, 2].map((c) => Math.hypot(...col(c)));
    const r = [0, 1, 2].map((c) => col(c).map((v) => v / (s[c] || 1)));
    // Rotation matrix (columns r[0], r[1], r[2]) to quaternion.
    const [m00, m10, m20] = r[0], [m01, m11, m21] = r[1], [m02, m12, m22] = r[2];
    const trace = m00 + m11 + m22;
    let q: number[];
    if (trace > 0) {
        const k = 0.5 / Math.sqrt(trace + 1);
        q = [(m21 - m12) * k, (m02 - m20) * k, (m10 - m01) * k, 0.25 / k];
    } else if (m00 > m11 && m00 > m22) {
        const k = 2 * Math.sqrt(1 + m00 - m11 - m22);
        q = [0.25 * k, (m01 + m10) / k, (m02 + m20) / k, (m21 - m12) / k];
    } else if (m11 > m22) {
        const k = 2 * Math.sqrt(1 + m11 - m00 - m22);
        q = [(m01 + m10) / k, 0.25 * k, (m12 + m21) / k, (m02 - m20) / k];
    } else {
        const k = 2 * Math.sqrt(1 + m22 - m00 - m11);
        q = [(m02 + m20) / k, (m12 + m21) / k, 0.25 * k, (m10 - m01) / k];
    }
    return { t, r: q, s };
}

export interface ModelPlacement {
    world: Transform;
}

/** A shown/hidden group of models (follows the same toggles as the stand-in it replaces). */
export class ModelGroup {
    readonly models: Model[] = [];
    private shown = true;
    setShown(shown: boolean): void {
        this.shown = shown;
        for (const m of this.models) m.show = shown;
    }
    add(model: Model): void {
        model.show = this.shown;
        this.models.push(model);
    }
    /** Plays (or freezes) any animation the exported models carry: idles and gestures stop under Less movement. */
    setAnimating(animate: boolean): void {
        this.animating = animate;
        for (const m of this.models) {
            if (!m.ready) continue;
            // Removing the animations leaves each figure in its current pose.
            if (!animate) m.activeAnimations.removeAll();
            else if (m.activeAnimations.length === 0 && hasAnimations(m)) m.activeAnimations.addAll({ loop: ModelAnimationLoop.REPEAT });
        }
    }
    animating = true;
}

/** The exported models listed in models/manifest.json and their Unreal bounds (models/bounds.json). */
export class ModelLibrary {
    private readonly glbs = new Map<string, Promise<{ glb: Glb; axes: Matrix4 } | null>>();

    constructor(private readonly scene: Scene, private readonly geo: Geo, private readonly manifest: Record<string, string>,
        private readonly bounds: Record<string, UnrealBounds>) {}

    has(asset: string | undefined): asset is string {
        return asset !== undefined && asset in this.manifest && asset in this.bounds;
    }

    private load(asset: string): Promise<{ glb: Glb; axes: Matrix4 } | null> {
        let pending = this.glbs.get(asset);
        if (!pending) {
            pending = fetch(this.manifest[asset]).then((r) => r.arrayBuffer()).then((data) => {
                const glb = parseGlb(data);
                const own = glbBounds(glb.json);
                if (!own) throw new Error("no POSITION bounds");
                const fit = fitAxes(own, this.bounds[asset]);
                if (!fit.reflection) console.warn(`Model ${asset}: best axis fit is a rotation, not the expected reflection; check its orientation.`);
                return { glb, axes: fit.matrix };
            }).catch((error) => {
                console.warn(`Model ${asset} could not be used; keeping the stand-in.`, error);
                return null;
            });
            this.glbs.set(asset, pending);
        }
        return pending;
    }

    /**
     * Unreal world cm -> Earth-fixed: the shared base for every exported model. Unreal's Y points south, so a point (x, y, z)
     * is (x, -y, z) / 100 east-north-up metres. (ueTransformToLocalMatrix flips both sides, for content already mirrored
     * into metres; the GLBs here are mapped into Unreal space by their axis fit instead.)
     */
    private base(): Matrix4 {
        return Matrix4.multiply(this.geo.enuToFixed, Matrix4.fromScale(new Cartesian3(0.01, -0.01, 0.01)), new Matrix4());
    }

    /** Draws `asset` at each world transform: one instanced model for many copies (the lot's cars), else one model each. */
    async place(asset: string, worlds: Transform[], group: ModelGroup, options: { animate?: boolean } = {}): Promise<boolean> {
        const loaded = await this.load(asset);
        if (!loaded || worlds.length === 0) return false;
        const { glb, axes } = loaded;
        const skinned = (glb.json.nodes ?? []).some((n) => n.skin !== undefined);
        // Unreal world = W * A * p. With A outside: W * A = A * (A^-1 W A), and A^-1 W A is a proper transform.
        const inverseAxes = Matrix4.inverse(axes, new Matrix4());
        const modelMatrix = Matrix4.multiply(this.base(), axes, new Matrix4());
        const instanceMatrices = worlds.map((w) => {
            const W = unrealMatrix(w);
            return Matrix4.multiply(inverseAxes, Matrix4.multiply(W, axes, new Matrix4()), new Matrix4());
        });
        const make = (gltf: Uint8Array, matrix: Matrix4) => fromBytes(gltf, matrix, asset);
        if (skinned || worlds.length < INSTANCE_FROM) {
            // One model per placement: skinned meshes can't be GPU-instanced, and a handful of copies don't need it. CesiumJS
            // culls an instanced primitive by its vertex range plus the instance translations, ignoring instance scale and
            // rotation, so a scaled prop (the trailer at 0.065, the barriers at 0.103) would be culled while in view.
            const bytes = writeGlb(glb);
            for (const J of instanceMatrices) {
                const model = await make(bytes, Matrix4.multiply(modelMatrix, J, new Matrix4()));
                if ((glb.json.animations?.length ?? 0) > 0) {
                    animated.add(model);
                    model.readyEvent.addEventListener(() => {
                        if (group.animating && options.animate !== false) model.activeAnimations.addAll({ loop: ModelAnimationLoop.REPEAT });
                    });
                }
                this.scene.primitives.add(model);
                group.add(model);
            }
            return true;
        }
        const model = await make(writeGlb(instanceGlb(glb, instanceMatrices)), modelMatrix);
        this.scene.primitives.add(model);
        group.add(model);
        return true;
    }

    /** A single movable model (a cue's car or person): returns a setter for its Unreal world transform. */
    async movable(asset: string, group: ModelGroup): Promise<((world: Transform) => void) | null> {
        const loaded = await this.load(asset);
        if (!loaded) return null;
        const model = await fromBytes(writeGlb(loaded.glb), Matrix4.IDENTITY, asset);
        this.scene.primitives.add(model);
        group.add(model);
        const base = this.base();
        return (world) => {
            Matrix4.multiply(base, Matrix4.multiply(unrealMatrix(world), loaded.axes, new Matrix4()), model.modelMatrix);
        };
    }
}

/** A model from GLB bytes, in the GLB's own axes (no up/forward correction: the bounds fit handles axes). */
async function fromBytes(bytes: Uint8Array, modelMatrix: Matrix4, id: string): Promise<Model> {
    const url = URL.createObjectURL(new Blob([bytes as BlobPart], { type: "model/gltf-binary" }));
    const model = await Model.fromGltfAsync({
        url, id, modelMatrix: Matrix4.clone(modelMatrix), upAxis: Axis.Z, forwardAxis: Axis.X, shadows: ShadowMode.ENABLED, allowPicking: false,
    });
    model.readyEvent.addEventListener(() => URL.revokeObjectURL(url));
    return model;
}

/** Placements from which a mesh is GPU-instanced (the lot's cars: unscaled and centred on their pivots, so culled right). */
const INSTANCE_FROM = 32;

const animated = new WeakSet<Model>();
const hasAnimations = (m: Model) => animated.has(m);

/** An Unreal transform as a plain matrix in Unreal's own axes (cm). */
function unrealMatrix(t: Transform): Matrix4 {
    const x = t.rotation.rotateVector(new Vec3(t.scale3D.x, 0, 0));
    const y = t.rotation.rotateVector(new Vec3(0, t.scale3D.y, 0));
    const z = t.rotation.rotateVector(new Vec3(0, 0, t.scale3D.z));
    const p = t.translation;
    return Matrix4.fromArray([x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, p.x, p.y, p.z, 1]);
}
