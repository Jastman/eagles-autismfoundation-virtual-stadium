// Turns the prop and eagle models in Assets/ (git-ignored; the same files Tools/Editor/import_lff_props.py and
// Tools/Pipeline/prepare_eagle.py bring into Unreal) into web-ready GLBs in web/models/, which are committed so every
// web build has them:
//
//   Assets/bag.gltf             ->  web/models/SM_LFF_Bag.glb             (clear bag, step 6)
//   Assets/barrier.gltf         ->  web/models/SM_LFF_Barrier.glb         (queue barriers, step 4)
//   Assets/sensoryVehicle.gltf  ->  web/models/SM_LFF_SensoryVehicle.glb  (Sensory Activation Vehicle, step 11; with
//                                   the photo-based side wrap from Tools/Pipeline/prepare_sav_wrap.py when it exists)
//   Assets/bald-eagle/source/Bald Eagle 05a.gltf  ->  web/models/eagle.glb + eagle.orient.json  (the anthem flyover, with
//                                   its flap loop)
//
// Each prop GLB gets <name>.bounds.json with the Unreal mesh's own bounds (web/data/materials.json, from the level), so
// scene/models.ts can fit the glTF's axes and units to Unreal's and draw it at the level's transforms. Geometry is welded
// and deduplicated and textures are capped at 1024 px (iPhone memory). The clear bag's plastic, invisible in the source
// file (alpha 0), gets a faint two-sided clear finish like Unreal's M_LFF_ClearBag.
//
// The eagle is turned, scaled and centred as prepare_eagle.py does for Unreal (head toward +X, 2.8 m wingspan, Z up) by
// a matrix written next to it (eagle.orient.json, applied by scene/eagle.ts), loses the feather cards that have a texture but no UVs (transparent in Unreal), and keeps its own 2 s flap
// animation (CesiumJS plays the skins and node animation that Unreal needed baked
// into a geometry cache).
//
//   node tools/prepare_models.mjs
import { NodeIO } from "@gltf-transform/core";
import { ALL_EXTENSIONS } from "@gltf-transform/extensions";
import { dedup, prune, textureCompress, unpartition, weld } from "@gltf-transform/functions";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import sharp from "sharp";

const web = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repo = resolve(web, "..");
const assets = join(repo, "Assets");
const out = join(web, "models");
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const TEXTURE_MAX = 1024;
const EAGLE_WINGSPAN_M = 2.8; // prepare_eagle.py WINGSPAN_CM

const PROPS = [
    { source: "bag.gltf", asset: "SM_LFF_Bag" },
    { source: "barrier.gltf", asset: "SM_LFF_Barrier" },
    { source: "sensoryVehicle.gltf", asset: "SM_LFF_SensoryVehicle" },
];

const meshBounds = JSON.parse(readFileSync(join(web, "data", "materials.json"), "utf8")).staticMeshes ?? {};

function unrealBounds(asset) {
    const entry = Object.entries(meshBounds).find(([path]) => path.endsWith(`/${asset}.${asset}`))?.[1];
    const b = entry?.extendedBounds;
    if (!b) throw new Error(`no Unreal bounds for ${asset} in web/data/materials.json`);
    const o = [b.Origin.x, b.Origin.y, b.Origin.z], e = [b.BoxExtent.x, b.BoxExtent.y, b.BoxExtent.z];
    return { asset: `/Game/...${asset}`, min: o.map((v, i) => v - e[i]), max: o.map((v, i) => v + e[i]), units: "cm", frame: "unreal-local" };
}

/** The clear bag: the transparent plastic (alpha 0 in the source) becomes faintly visible from both sides. */
function clearBag(doc) {
    for (const node of doc.getRoot().listNodes()) {
        if (node.getName() !== "Cube" || !node.getMesh()) continue;
        for (const prim of node.getMesh().listPrimitives()) {
            const material = prim.getMaterial();
            if (!material || material.getAlphaMode() !== "BLEND") continue;
            const c = material.getBaseColorFactor();
            material.setBaseColorFactor([Math.max(c[0], 0.85), Math.max(c[1], 0.88), Math.max(c[2], 0.9), 0.22])
                .setRoughnessFactor(0.15).setMetallicFactor(0).setDoubleSided(true);
        }
    }
}

/** The trailer's side wrap, as Unreal's import uses it: Tools/Pipeline/prepare_sav_wrap.py's photo-rectified
 *  Saved/PropImport/T_LFF_SAV_Wrap.jpg replaces the model's own wrap (image 1) when it has been generated. */
function savWrap(doc) {
    const wrap = join(repo, "Saved", "PropImport", "T_LFF_SAV_Wrap.jpg");
    const texture = doc.getRoot().listTextures()[1];
    if (!existsSync(wrap) || !texture) return false;
    texture.setImage(readFileSync(wrap)).setMimeType("image/jpeg");
    return true;
}

async function slim(doc) {
    await doc.transform(unpartition(), dedup(), prune(), weld(),
        textureCompress({ encoder: sharp, resize: [TEXTURE_MAX, TEXTURE_MAX], quality: 85 }));
}

const mat4 = {
    mul(a, b) {
        const r = new Array(16).fill(0);
        for (let c = 0; c < 4; ++c) for (let row = 0; row < 4; ++row) for (let k = 0; k < 4; ++k) r[c * 4 + row] += a[k * 4 + row] * b[c * 4 + k];
        return r;
    },
    point(m, p) {
        return [0, 1, 2].map((i) => m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i]);
    },
};

/** Rest-pose world bounds and the head's position: the same cues prepare_eagle.py reads in Blender. */
function eagleRest(doc) {
    const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
    let head = null;
    for (const node of doc.getRoot().listNodes()) {
        if (!head && /HEAD/i.test(node.getName())) head = node.getWorldTranslation();
        const mesh = node.getMesh();
        if (!mesh) continue;
        // Skinned vertices are already in the bind pose's world space; rigid parts follow their node.
        const world = node.getSkin() ? [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] : node.getWorldMatrix();
        for (const prim of mesh.listPrimitives()) {
            const pos = prim.getAttribute("POSITION");
            const v = [0, 0, 0];
            for (let i = 0; i < pos.getCount(); ++i) {
                const w = mat4.point(world, pos.getElement(i, v));
                for (let k = 0; k < 3; ++k) {
                    lo[k] = Math.min(lo[k], w[k]);
                    hi[k] = Math.max(hi[k], w[k]);
                }
            }
        }
    }
    return { lo, hi, head };
}

/**
 * Drops primitives whose material samples a texture they have no UVs for (the eagle's 30-odd head and leg feather cards).
 * CesiumJS can't compile a shader for them, which stops the whole picture; in Unreal they sample texel (0, 0) of a masked
 * feather texture, which is fully transparent, so nothing visible goes.
 */
function dropUnmappedTextured(doc) {
    let dropped = 0;
    for (const mesh of doc.getRoot().listMeshes()) {
        for (const prim of mesh.listPrimitives()) {
            const material = prim.getMaterial();
            const textured = material && (material.getBaseColorTexture() || material.getNormalTexture() || material.getEmissiveTexture()
                || material.getMetallicRoughnessTexture() || material.getOcclusionTexture());
            if (textured && !prim.getAttribute("TEXCOORD_0")) {
                mesh.removePrimitive(prim);
                prim.dispose();
                ++dropped;
            }
        }
    }
    return dropped;
}

/**
 * The eagle's orientation: turned to fly along +X with +Z up, 2.8 m across, centred on its origin. Returned as a matrix for
 * the web to apply after CesiumJS's own Y-up to Z-up correction (models/eagle.orient.json), not baked into the file: a
 * root node above the rig breaks CesiumJS's skinning (the body vanishes and the feathers drift).
 */
function orientEagle(doc) {
    const { lo, hi, head } = eagleRest(doc);
    // glTF is Y-up: in a Z-up frame a point (x, y, z) is (x, -z, y) (CesiumJS's upAxis: Y correction).
    const zUp = [1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1];
    const centre = lo.map((v, i) => (v + hi[i]) / 2);
    const c = mat4.point(zUp, centre);
    const h = head ? mat4.point(zUp, head) : [c[0] + 1, c[1], c[2]];
    const yaw = Math.atan2(h[1] - c[1], h[0] - c[0]);
    const cos = Math.cos(-yaw), sin = Math.sin(-yaw);
    const turn = [cos, sin, 0, 0, -sin, cos, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
    const turned = mat4.mul(turn, zUp);
    // Wingspan: the turned model's extent across (Y), from its eight bound corners.
    const ys = [];
    for (let corner = 0; corner < 8; ++corner) {
        const p = [0, 1, 2].map((k) => ((corner >> k) & 1 ? hi[k] : lo[k]));
        ys.push(mat4.point(turned, p)[1]);
    }
    const span = Math.max(...ys) - Math.min(...ys);
    const s = EAGLE_WINGSPAN_M / Math.max(span, 1e-6);
    const tc = mat4.point(turned, centre);
    const matrix = mat4.mul([s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, -tc[0] * s, -tc[1] * s, -tc[2] * s, 1], turn);
    return { matrix, yawDeg: (-yaw * 180) / Math.PI, scale: s, restSpanSource: span };
}

mkdirSync(out, { recursive: true });
for (const { source, asset } of PROPS) {
    const src = join(assets, source);
    if (!existsSync(src)) {
        console.warn(`prepare_models: ${source} not in Assets/, skipped (the committed ${asset}.glb stays).`);
        continue;
    }
    const doc = await io.read(src);
    if (asset === "SM_LFF_Bag") clearBag(doc);
    if (asset === "SM_LFF_SensoryVehicle") console.log(`prepare_models: trailer wrap ${savWrap(doc) ? "from Saved/PropImport/T_LFF_SAV_Wrap.jpg" : "is the model's own (G6)"}`);
    await slim(doc);
    await io.write(join(out, `${asset}.glb`), doc);
    writeFileSync(join(out, `${asset}.bounds.json`), JSON.stringify(unrealBounds(asset), null, 2) + "\n");
    console.log(`prepare_models: ${asset}.glb (${Math.round(readFileSync(join(out, `${asset}.glb`)).length / 1024)} KB)`);
}

const eagleSrc = join(assets, "bald-eagle", "source", "Bald Eagle 05a.gltf");
if (existsSync(eagleSrc)) {
    const doc = await io.read(eagleSrc);
    const dropped = dropUnmappedTextured(doc);
    const info = orientEagle(doc);
    await slim(doc);
    await io.write(join(out, "eagle.glb"), doc);
    writeFileSync(join(out, "eagle.orient.json"), JSON.stringify({
        note: "Applied after CesiumJS's upAxis: Y correction (tools/prepare_models.mjs): head toward +X, Z up, 2.8 m wingspan, centred.",
        matrix: info.matrix.map((v) => Number(v.toFixed(7))),
    }, null, 2) + "\n");
    console.log(`prepare_models: eagle.glb (${Math.round(readFileSync(join(out, "eagle.glb")).length / 1024)} KB), turned ` +
        `${info.yawDeg.toFixed(1)} deg, scale ${info.scale.toFixed(5)}, ${dropped} unmapped feather cards dropped`);
} else {
    console.warn("prepare_models: Assets/bald-eagle not found, eagle skipped (the committed eagle.glb stays).");
}
