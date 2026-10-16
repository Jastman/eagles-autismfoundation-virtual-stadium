/// <reference types="node" />
// The committed models (web/models, from tools/prepare_models.mjs) line up with the Unreal meshes they replace.
import { readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { fitAxes, glbBounds, parseGlb } from "../src/scene/models";

const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "models");
const props = readdirSync(dir).filter((f: string) => f.endsWith(".bounds.json")).map((f: string) => f.replace(".bounds.json", ""));

describe("committed prop models", () => {
    it.each(props)("%s fits its Unreal bounds", (asset: string) => {
        const bytes = readFileSync(join(dir, `${asset}.glb`));
        const glb = parseGlb(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer);
        const own = glbBounds(glb.json)!;
        const unreal = JSON.parse(readFileSync(join(dir, `${asset}.bounds.json`), "utf8"));
        const fit = fitAxes(own, unreal);
        const size = [0, 1, 2].map((k) => unreal.max[k] - unreal.min[k]);
        // Within 2% of the mesh's size on every face of the box, and a left/right-handed conversion as expected.
        expect(fit.error).toBeLessThan(0.02 * 6 * Math.max(...size));
        expect(fit.reflection).toBe(true);
    });
});
