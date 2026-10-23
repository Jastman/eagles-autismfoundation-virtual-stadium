// Shrinks the stadium tilesets' texture memory (Docs/WEB_PARITY.md W16): downloads the ion assets, re-encodes every
// texture as KTX2 (Basis ETC1S with mipmaps, which stays compressed on the GPU: ETC2/ASTC on iPad, BC on desktops),
// capped at 2048 px and resized to powers of two, and writes the tilesets plus a zip per asset ready to upload to Cesium
// ion as "3D Tiles". Geometry (Draco), batch tables and the tile tree are copied unchanged.
//
// Measured on the stadium structure (6040628): 285 textures, 32 of them 4096 px, ~4.5 GB of GPU memory as RGBA becomes
// ~0.3 GB; the download goes from 160 MB to 60 MB.
//
//   node tools/compress_stadium_tiles.mjs [assetId ...]      (default: VITE_STADIUM_ASSET_IDS, or the three stadium assets)
//
// Reads VITE_STADIUM_ION_TOKEN (or VITE_CESIUM_ION_TOKEN) from web/.env.local. Output, never committed (the model is
// licensed): web/assets-src/stadium-tiles/<id>/ and <id>.zip. Re-runs skip finished tiles. Environment: MAX (px, 2048),
// QUALITY (ETC1S 1-255, 192), JOBS (parallel encoders, CPU count). Behind a proxy, Node needs NODE_USE_ENV_PROXY=1.
import { execFileSync, fork } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { availableParallelism } from "node:os";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import sharp from "sharp";
import { glbsOf, parseTile, readGlb, writeGlb, writeTile } from "./tiles3d.mjs";

const web = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const outRoot = join(web, "assets-src", "stadium-tiles");
const MAX = Number(process.env.MAX || 2048), QUALITY = Number(process.env.QUALITY || 192);

const walk = (d) => readdirSync(d).flatMap((f) => { const p = join(d, f); return statSync(p).isDirectory() ? walk(p) : [p]; }).sort();
const pow2 = (n) => Math.max(4, Math.min(MAX, 2 ** Math.round(Math.log2(n))));

function readEnv() {
    const file = join(web, ".env.local");
    if (!existsSync(file)) return {};
    return Object.fromEntries(readFileSync(file, "utf8").split("\n").filter((l) => l.includes("=") && !l.trim().startsWith("#"))
        .map((l) => [l.slice(0, l.indexOf("=")).trim(), l.slice(l.indexOf("=") + 1).trim().replace(/^"|"$/g, "")]));
}

async function get(url, headers, tries = 5) {
    for (let i = 0; ; i++) {
        try {
            const r = await fetch(url, { headers });
            if (!r.ok) throw new Error(`${r.status} ${url}`);
            return new Uint8Array(await r.arrayBuffer());
        } catch (error) {
            if (i >= tries) throw error;
            await new Promise((done) => setTimeout(done, 1000 * 2 ** i));
        }
    }
}

/** Every file of an ion 3D Tiles asset into <dir>, keeping its relative paths. */
async function download(id, token, dir) {
    const ep = JSON.parse(new TextDecoder().decode(await get(`https://api.cesium.com/v1/assets/${id}/endpoint`, { Authorization: `Bearer ${token}` })));
    if (ep.type !== "3DTILES") throw new Error(`asset ${id} is ${ep.type}, not 3D Tiles`);
    const headers = { Authorization: `Bearer ${ep.accessToken}` };
    const base = new URL(ep.url);
    const tilesets = [{ rel: base.pathname.split("/").pop(), url: base }], contents = [], seen = new Set();
    while (tilesets.length) {
        const { rel, url } = tilesets.shift();
        const data = await get(url, headers);
        mkdirSync(dirname(join(dir, rel)), { recursive: true });
        writeFileSync(join(dir, rel), data);
        const visit = (tile) => {
            for (const content of [tile.content, ...(tile.contents ?? [])].filter(Boolean)) {
                const uri = content.uri ?? content.url;
                const u = new URL(uri, url);
                for (const [k, v] of base.searchParams) if (!u.searchParams.has(k)) u.searchParams.set(k, v);
                const path = join(dirname(rel), decodeURIComponent(uri.split("?")[0]));
                if (seen.has(path)) continue;
                seen.add(path);
                (/\.json$/i.test(path) ? tilesets : contents).push({ rel: path, url: u });
            }
            for (const child of tile.children ?? []) visit(child);
        };
        visit(JSON.parse(new TextDecoder().decode(data)).root);
    }
    let next = 0;
    const worker = async () => {
        while (next < contents.length) {
            const { rel, url } = contents[next++];
            if (existsSync(join(dir, rel))) continue;
            const data = await get(url, headers);
            mkdirSync(dirname(join(dir, rel)), { recursive: true });
            writeFileSync(join(dir, rel), data);
        }
    };
    await Promise.all(Array.from({ length: 12 }, worker));
    return contents.length;
}

/** Re-encodes the images of one tile file; returns byte and estimated GPU-memory totals. */
async function compressTile(src, dst, encodeToKTX2) {
    const decoder = async (buffer) => {
        const { data, info } = await sharp(buffer).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
        return { data: new Uint8Array(data), width: info.width, height: info.height };
    };
    const input = readFileSync(src);
    const tile = parseTile(input);
    const stats = { images: 0, gpuBefore: 0, gpuAfter: 0 };
    for (const glb of glbsOf(tile)) {
        const { json, bin } = readGlb(glb.get());
        if (!json.images?.length) continue;
        const views = json.bufferViews.map((v) => bin.subarray(v.byteOffset ?? 0, (v.byteOffset ?? 0) + v.byteLength));
        let changed = false;
        for (const [index, image] of json.images.entries()) {
            if (image.bufferView === undefined || image.mimeType === "image/ktx2") continue;
            const raw = views[image.bufferView];
            const meta = await sharp(raw).metadata();
            const w = pow2(meta.width), h = pow2(meta.height);
            const png = await sharp(raw).resize(w, h, { fit: "fill", kernel: "lanczos3" }).png().toBuffer();
            views[image.bufferView] = await encodeToKTX2(new Uint8Array(png), {
                isUASTC: false, qualityLevel: QUALITY, compressionLevel: 2, generateMipmap: true, isPerceptual: true, isYFlip: false,
                needSupercompression: true, enableDebug: false, imageDecoder: decoder,
            });
            image.mimeType = "image/ktx2";
            for (const texture of json.textures ?? []) {
                if (texture.source !== index) continue;
                texture.extensions = { ...(texture.extensions ?? {}), KHR_texture_basisu: { source: index } };
                delete texture.source;
            }
            stats.images++;
            stats.gpuBefore += (meta.width * meta.height * 4 * 4) / 3;
            stats.gpuAfter += (w * h * (meta.hasAlpha ? 1 : 0.5) * 4) / 3;
            changed = true;
        }
        if (!changed) continue;
        for (const list of ["extensionsUsed", "extensionsRequired"]) json[list] = [...new Set([...(json[list] ?? []), "KHR_texture_basisu"])];
        // The binary chunk again: every buffer view in order, 8-byte aligned.
        let at = 0;
        const placed = json.bufferViews.map((view, k) => {
            at = (at + 7) & ~7;
            view.byteOffset = at;
            view.byteLength = views[k].length;
            const entry = [at, views[k]];
            at += views[k].length;
            return entry;
        });
        const nextBin = new Uint8Array((at + 7) & ~7);
        for (const [offset, bytes] of placed) nextBin.set(bytes, offset);
        json.buffers = [{ ...json.buffers[0], byteLength: nextBin.length }];
        glb.set(writeGlb(json, nextBin));
    }
    const output = writeTile(tile);
    mkdirSync(dirname(dst), { recursive: true });
    writeFileSync(dst, output);
    return { ...stats, before: input.length, after: output.length };
}

if (process.argv[2] === "--worker") {
    // A child: compresses the files it is sent, one at a time.
    const { encodeToKTX2 } = await import("ktx2-encoder");
    // The Basis encoder reports every texture on stdout.
    for (const stream of [process.stdout, process.stderr]) {
        const write = stream.write.bind(stream);
        stream.write = (chunk, ...rest) => (/^(basis_compressor::process|Mode: ETC1S)/.test(String(chunk)) ? true : write(chunk, ...rest));
    }
    const log = console.log;
    console.log = (...args) => { if (!/^(basis_compressor::process|Mode: ETC1S)/.test(String(args[0]))) log(...args); };
    process.on("message", async ({ src, dst }) => {
        try {
            process.send({ ok: true, ...(await compressTile(src, dst, encodeToKTX2)) });
        } catch (error) {
            process.send({ ok: false, error: `${src}: ${error.message}` });
        }
    });
} else {
    const env = readEnv();
    const token = process.env.VITE_STADIUM_ION_TOKEN || env.VITE_STADIUM_ION_TOKEN || env.VITE_CESIUM_ION_TOKEN;
    const ids = process.argv.slice(2).length > 0 ? process.argv.slice(2)
        : (env.VITE_STADIUM_ASSET_IDS || "6040628,6040635,6040637").split(",").map((s) => s.trim());
    const jobs = Number(process.env.JOBS || availableParallelism());
    for (const id of ids) {
        const source = join(outRoot, "source", id), target = join(outRoot, id);
        if (!existsSync(join(source, "tileset.json"))) {
            if (!token) throw new Error("No ion token: set VITE_STADIUM_ION_TOKEN in web/.env.local.");
            console.log(`compress_stadium_tiles: downloading asset ${id}...`);
            await download(id, token, source);
        }
        const files = walk(source);
        const todo = [];
        for (const file of files) {
            const dst = join(target, relative(source, file));
            if (file.endsWith(".json")) {
                mkdirSync(dirname(dst), { recursive: true });
                copyFileSync(file, dst);
            } else if (!existsSync(dst)) todo.push({ src: file, dst });
        }
        const totals = { images: 0, before: 0, after: 0, gpuBefore: 0, gpuAfter: 0 };
        let done = 0;
        await Promise.all(Array.from({ length: Math.min(jobs, todo.length) }, () => new Promise((finish, fail) => {
            const child = fork(fileURLToPath(import.meta.url), ["--worker"]);
            const sendNext = () => (todo.length > 0 ? child.send(todo.shift()) : (child.kill(), finish()));
            child.on("message", (result) => {
                if (!result.ok) return fail(new Error(result.error));
                for (const k of Object.keys(totals)) totals[k] += result[k];
                if (++done % 25 === 0) console.log(`compress_stadium_tiles: ${id}: ${done} tiles...`);
                sendNext();
            });
            child.on("error", fail);
            sendNext();
        })));
        const mb = (n) => `${(n / 1048576).toFixed(1)} MB`;
        console.log(`compress_stadium_tiles: ${id}: ${done} tiles, ${totals.images} textures; download ${mb(totals.before)} -> ` +
            `${mb(totals.after)}; texture GPU memory about ${mb(totals.gpuBefore)} -> ${mb(totals.gpuAfter)}`);
        const zip = join(outRoot, `${id}.zip`);
        rmSync(zip, { force: true });
        execFileSync("zip", ["-qr", "-0", zip, "."], { cwd: target });
        console.log(`compress_stadium_tiles: ${zip}: upload to Cesium ion as 3D Tiles, then use the new asset ID.`);
    }
}
