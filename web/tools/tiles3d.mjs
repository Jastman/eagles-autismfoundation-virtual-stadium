// 3D Tiles 1.0 containers for tools/compress_stadium_tiles.mjs: split b3dm / i3dm / cmpt / glb tiles into their embedded GLBs
// and put them back together, every other byte (feature and batch tables) unchanged.
const pad8 = (n) => (n + 7) & ~7;
const u32 = (b, o) => new DataView(b.buffer, b.byteOffset).getUint32(o, true);
export function magic(b, o = 0) { return String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]); }

/** Parses a tile into a tree: { kind, header, tables, glb } or { kind: 'cmpt', tiles: [...] }. */
export function parseTile(b) {
    const m = magic(b);
    if (m === "glTF") return { kind: "glb", glb: b.subarray(0, u32(b, 8)) };
    if (m === "cmpt") {
        const n = u32(b, 12), tiles = [];
        let at = 16;
        for (let i = 0; i < n; i++) { const len = u32(b, at + 8); tiles.push(parseTile(b.subarray(at, at + len))); at += len; }
        return { kind: "cmpt", version: u32(b, 4), tiles };
    }
    if (m === "b3dm" || m === "i3dm") {
        const headerLen = m === "b3dm" ? 28 : 32, len = u32(b, 8);
        const tableLen = u32(b, 12) + u32(b, 16) + u32(b, 20) + u32(b, 24);
        const gltfFormat = m === "i3dm" ? u32(b, 28) : 1;
        if (gltfFormat !== 1) return { kind: "raw", bytes: b.subarray(0, len) };
        return { kind: m, header: b.slice(0, headerLen), tables: b.subarray(headerLen, headerLen + tableLen), glb: b.subarray(headerLen + tableLen, len) };
    }
    return { kind: "raw", bytes: b };
}

/** All GLBs in a parsed tile, with a setter to replace each. */
export function glbsOf(tile) {
    if (tile.kind === "cmpt") return tile.tiles.flatMap(glbsOf);
    if (tile.glb) return [{ get: () => tile.glb, set: (g) => { tile.glb = g; } }];
    return [];
}

export function writeTile(tile) {
    if (tile.kind === "glb") return tile.glb;
    if (tile.kind === "raw") return tile.bytes;
    if (tile.kind === "cmpt") {
        const parts = tile.tiles.map(writeTile);
        const len = 16 + parts.reduce((n, p) => n + p.length, 0);
        const out = new Uint8Array(len), dv = new DataView(out.buffer);
        out.set([0x63, 0x6d, 0x70, 0x74]); dv.setUint32(4, tile.version, true); dv.setUint32(8, len, true); dv.setUint32(12, parts.length, true);
        let at = 16; for (const p of parts) { out.set(p, at); at += p.length; }
        return out;
    }
    // b3dm / i3dm: the tables are already padded so the GLB starts on 8 bytes; pad the end to 8 as the spec asks.
    const len = pad8(tile.header.length + tile.tables.length + tile.glb.length);
    const out = new Uint8Array(len);
    out.set(tile.header); out.set(tile.tables, tile.header.length); out.set(tile.glb, tile.header.length + tile.tables.length);
    new DataView(out.buffer).setUint32(8, len, true);
    return out;
}

export function readGlb(g) {
    const jsonLen = u32(g, 12);
    const json = JSON.parse(new TextDecoder().decode(g.subarray(20, 20 + jsonLen)));
    const binAt = 20 + jsonLen;
    const bin = binAt + 8 <= g.length && u32(g, binAt + 4) === 0x004e4942 ? g.subarray(binAt + 8, binAt + 8 + u32(g, binAt)) : new Uint8Array(0);
    return { json, bin };
}

export function writeGlb(json, bin) {
    const enc = new TextEncoder().encode(JSON.stringify(json));
    const jsonLen = (enc.length + 3) & ~3, binLen = (bin.length + 3) & ~3;
    const total = 12 + 8 + jsonLen + (bin.length ? 8 + binLen : 0);
    const out = new Uint8Array(total), dv = new DataView(out.buffer);
    dv.setUint32(0, 0x46546c67, true); dv.setUint32(4, 2, true); dv.setUint32(8, total, true);
    dv.setUint32(12, jsonLen, true); dv.setUint32(16, 0x4e4f534a, true); out.set(enc, 20); out.fill(0x20, 20 + enc.length, 20 + jsonLen);
    if (bin.length) { const at = 20 + jsonLen; dv.setUint32(at, binLen, true); dv.setUint32(at + 4, 0x004e4942, true); out.set(bin, at + 8); }
    return out;
}
