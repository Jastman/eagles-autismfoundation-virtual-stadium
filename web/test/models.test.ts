// The exported-model path: GLB read/write, bounds, the axis fit against Unreal bounds, and instancing.
import { Matrix4 } from "cesium";
import { describe, expect, it } from "vitest";
import { fitAxes, glbBounds, instanceGlb, parseGlb, writeGlb } from "../src/scene/models";

/** A one-triangle GLB whose POSITION accessor spans the given box. */
function testGlb(min: number[], max: number[], node: Record<string, unknown> = {}) {
    const positions = new Float32Array([...min, max[0], min[1], min[2], ...max]);
    const bin = new Uint8Array(positions.buffer);
    const json = {
        asset: { version: "2.0" },
        scene: 0,
        scenes: [{ nodes: [0] }],
        nodes: [{ mesh: 0, ...node }],
        meshes: [{ primitives: [{ attributes: { POSITION: 0 } }] }],
        accessors: [{ bufferView: 0, componentType: 5126, count: 3, type: "VEC3", min, max }],
        bufferViews: [{ buffer: 0, byteOffset: 0, byteLength: bin.length }],
        buffers: [{ byteLength: bin.length }],
    };
    return writeGlb({ json, bin });
}

describe("exported models", () => {
    it("round-trips a GLB", () => {
        const bytes = testGlb([-1, 0, -2], [1, 1.5, 2]);
        const glb = parseGlb(bytes.buffer.slice(0) as ArrayBuffer);
        expect(glb.json.accessors?.[0].count).toBe(3);
        expect(glb.bin.length).toBe(36);
        expect(glbBounds(glb.json)).toEqual({ min: [-1, 0, -2], max: [1, 1.5, 2] });
    });

    it("applies node transforms to the bounds", () => {
        const glb = parseGlb(testGlb([0, 0, 0], [1, 1, 1], { translation: [10, 0, 0], scale: [2, 2, 2] }).buffer.slice(0) as ArrayBuffer);
        expect(glbBounds(glb.json)).toEqual({ min: [10, 0, 0], max: [12, 2, 2] });
    });

    it("finds the glTF-to-Unreal axes from bounds", () => {
        // A car 4.6 m long (Unreal X), 1.9 m wide (Y), 1.45 m tall (Z), exported as glTF (Y up, Z forward), in metres,
        // slightly off-centre so the signs are decidable.
        const unreal = { min: [-220, -95, 0], max: [240, 95, 145] };
        const gltf = { min: [-0.95, 0, -2.2], max: [0.95, 1.45, 2.4] }; // x = Unreal Y (mirrored or not), y = Unreal Z, z = Unreal X
        const fit = fitAxes(gltf, unreal);
        expect(fit.error).toBeLessThan(1);
        expect(fit.reflection).toBe(true);
        const p = Matrix4.multiplyByPoint(fit.matrix, { x: 0.5, y: 1.0, z: 2.4 } as never, { x: 0, y: 0, z: 0 } as never) as unknown as { x: number; y: number; z: number };
        expect(p.x).toBeCloseTo(240);
        expect(p.z).toBeCloseTo(100);
    });

    it("adds an instance list to every mesh node", () => {
        const glb = parseGlb(testGlb([0, 0, 0], [1, 1, 1]).buffer.slice(0) as ArrayBuffer);
        const instances = [Matrix4.IDENTITY, Matrix4.fromTranslation({ x: 5, y: 0, z: 0 } as never)];
        const out = instanceGlb(glb, instances);
        const ext = (out.json.nodes![0].extensions as Record<string, { attributes: Record<string, number> }>).EXT_mesh_gpu_instancing;
        expect(Object.keys(ext.attributes).sort()).toEqual(["ROTATION", "SCALE", "TRANSLATION"]);
        const t = out.json.accessors![ext.attributes.TRANSLATION];
        const view = out.json.bufferViews![t.bufferView!];
        const values = new Float32Array(out.bin.buffer.slice(view.byteOffset!, view.byteOffset! + view.byteLength));
        expect(Array.from(values)).toEqual([0, 0, 0, 5, 0, 0]);
        expect(out.json.extensionsUsed).toContain("EXT_mesh_gpu_instancing");
        // And it is still a valid GLB.
        const again = parseGlb(writeGlb(out).buffer.slice(0) as ArrayBuffer);
        expect(again.json.accessors!.length).toBe(4);
    });
});
