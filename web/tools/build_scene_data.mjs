// Compacts the raw level extract (web/data/props.json, materials.json, camera_targets.json, camera_paths.json,
// georeference.json, labels.json, places.json; written by tools/extract_umap.py from Content/LFF/Levels/L_LFF_Stadium.umap)
// into web/data/scene.json: only what the web scene draws, in Unreal world coordinates (cm, X East, Y South, Z Up).
//
//   python3 web/tools/extract_umap.py && node web/tools/build_scene_data.mjs
//
// Shapes:  cube | cylinder | sphere | cone | plane    Unreal's /Engine/BasicShapes (100 cm unit shapes, pivot at centre)
//          box                                          a project mesh drawn as its stored bounds (until a glTF is exported)
//          car                                          a City Sample car body (Epic content: Unreal-only licence, never exported)
//          person                                       a MetaHuman (Unreal-only licence): drawn as a simple figure
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const data = join(resolve(dirname(fileURLToPath(import.meta.url)), ".."), "data");
const read = (name) => JSON.parse(readFileSync(join(data, name), "utf8"));

const props = read("props.json");
const materials = read("materials.json");
const targets = read("camera_targets.json");
const paths = read("camera_paths.json");
const geo = read("georeference.json");
const labels = read("labels.json");
const places = read("places.json");

// ---- quaternion helpers (Unreal conventions: Hamilton product, q * v * q^-1) ----
const qmul = (a, b) => [
    a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
    a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
    a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
];
const qinv = (q) => [-q[0], -q[1], -q[2], q[3]];
const qrot = (q, v) => {
    const r = qmul(qmul(q, [v[0], v[1], v[2], 0]), qinv(q));
    return [r[0], r[1], r[2]];
};
const vec = (o) => [o.x, o.y, o.z];
const quat = (o) => [o.x, o.y, o.z, o.w];
const round = (v, d = 3) => (Array.isArray(v) ? v.map((x) => round(x, d)) : Math.round(v * 10 ** d) / 10 ** d);
const xf = (t) => ({ l: round(vec(t.location), 2), q: round(quat(t.quat), 7), s: round(vec(t.scale), 5) });

/** The part's transform relative to the actor root, root scale excluded (the root's own scale stays on its parts). */
function relativeTo(root, world) {
    const rq = quat(root.quat);
    const d = [world.location.x - root.location.x, world.location.y - root.location.y, world.location.z - root.location.z];
    return { l: round(qrot(qinv(rq), d), 2), q: round(qmul(qinv(rq), quat(world.quat)), 7), s: round(vec(world.scale), 5) };
}

// ---- colours ----
const matTable = materials.materials;
function linearColour(path) {
    if (!path) return null;
    let m = matTable[path];
    for (let depth = 0; m && depth < 6; ++depth) {
        const v = m.vectorParameters ?? {};
        const c = v.Color ?? v.BaseColor ?? v.BaseTint ?? v.Tint ?? m.effectiveVectorParameters?.Color ?? m.effectiveVectorParameters?.BaseColor;
        if (c) return round([c.r, c.g, c.b], 4);
        m = m.parent ? matTable[m.parent] : null;
    }
    return null;
}
// Imported prop materials carry their colour in textures; these are hand-picked averages of the props' look.
const FALLBACK = {
    "/Game/LFF/Props/Barrier/": [0.35, 0.36, 0.38],
    "/Game/LFF/Props/MetalDetector/": [0.08, 0.08, 0.09],
    "/Game/LFF/Props/Bag/": [0.7, 0.75, 0.8],
    "/Game/LFF/Props/Concession/": [0.55, 0.08, 0.06],
    "/Game/LFF/Props/SensoryVehicle/": [0.02, 0.2, 0.22],
    "/Game/LFF/Props/SensoryRoom/": [0.45, 0.55, 0.65],
};
const fallbackColour = (mesh) => Object.entries(FALLBACK).find(([prefix]) => mesh.startsWith(prefix))?.[1] ?? [0.5, 0.5, 0.5];

const BASIC = { Cube: "cube", Cylinder: "cylinder", Sphere: "sphere", Cone: "cone", Plane: "plane" };
function shapeOf(mesh) {
    const basic = mesh.match(/^\/Engine\/BasicShapes\/(\w+)\./);
    if (basic) return BASIC[basic[1]] ?? null;
    if (mesh.includes("/CitySampleVehicles/")) return mesh.includes("_No_Wheel") ? "car" : null; // wheels and glass ride in the body
    if (mesh.includes("_BodyMesh")) return "person";
    if (mesh.startsWith("/MetaHumanCrowdContent/") || mesh.includes("/People/Hair/") || mesh.includes("_FaceMesh")) return null;
    if (mesh.startsWith("/Game/LFF/Stadium/")) return null; // the licensed stadium: a Cesium ion asset on the web (scene/world.ts)
    return "box";
}

function meshBounds(mesh) {
    const info = materials.staticMeshes?.[mesh];
    const b = info?.extendedBounds;
    return b ? { o: round(vec(b.Origin), 1), e: round(vec(b.BoxExtent), 1) } : null;
}

// ---- actors ----
const SKIP_CLASSES = new Set(["Brush", "Cesium3DTileset", "CesiumCameraManager", "CesiumCartographicPolygon", "CesiumCreditSystemBP_C",
    "CesiumGeoreference", "CesiumSunSky", "ExponentialHeightFog", "PostProcessVolume", "VolumetricCloud", "WorldSettings", "RectLight",
    "LFFCameraTarget", "LFFCameraPath", "LFFStadiumCrowdSpawner"]);

const cueByExport = new Map(read("cues.json").map((c) => [c.export, c]));
const actors = [];
let skippedStadium = 0;
for (const actor of props.actors) {
    if (SKIP_CLASSES.has(actor.class)) continue;
    if ((actor.folder ?? "").startsWith("LFF_SiteRoot/LFF_Model")) {
        skippedStadium++;
        continue;
    }
    const comps = actor.components ?? [];
    const root = comps.find((c) => c.isRoot)?.world ?? actor.world;
    if (!root) continue;
    const parts = [];
    let text = null;
    const boxes = [];
    for (const c of comps) {
        if (!c.world) continue;
        // Unreal's default box extent is 32 cm.
        if (c.class === "BoxComponent") boxes.push({ name: c.name, extent: round(vec(c.boxExtent ?? { x: 32, y: 32, z: 32 }), 1), world: xf(c.world) });
        if (c.class === "TextRenderComponent") {
            text = {
                text: (c.text ?? "").replaceAll("<br>", "\n"),
                worldSize: c.worldSize ?? 26,
                colour: c.textRenderColor ?? { r: 255, g: 255, b: 255, a: 255 },
                align: c.horizontalAlignment ?? "EHTA_Left",
                local: relativeTo(root, c.world),
            };
            continue;
        }
        const mesh = c.mesh ?? "";
        if (!mesh) continue;
        const shape = shapeOf(mesh);
        if (shape === null) continue;
        const hidden = c.bHiddenInGame === true || c.bVisible === false;
        const mats = (c.materials ?? []).filter(Boolean);
        const colour = mats.map(linearColour).find(Boolean) ?? (shape === "box" ? fallbackColour(mesh) : shape === "car" ? [0.3, 0.3, 0.32] : [0.5, 0.49, 0.47]);
        const part = { shape, colour, hidden: hidden || undefined };
        if (shape === "box") part.bounds = meshBounds(mesh) ?? { o: [0, 0, 50], e: [50, 50, 50] };
        if (shape === "box") part.mesh = mesh;
        if (Array.isArray(c.instances) && c.instances.length > 0) {
            part.instances = c.instances.map((i) => relativeTo(root, i.world));
        } else {
            Object.assign(part, relativeTo(root, c.world));
        }
        parts.push(part);
    }
    const tags = actor.tags ?? [];
    const cue = cueByExport.get(actor.export);
    const entry = { label: actor.label, class: actor.class, tags, root: xf(root), parts };
    if (text) entry.text = text;
    if (boxes.length > 0) entry.boxes = boxes;
    if (cue) {
        const p = cue.props ?? {};
        entry.cue = {
            id: cue.cueId ?? "",
            trigger: cue.trigger ?? "Arrival",
            start: xf(cue.startWorld ?? root),
            playSeconds: p.PlaySeconds ?? 3,
            playDelaySeconds: p.PlayDelaySeconds ?? 0,
            linear: p.bLinear ?? false,
            hiddenBefore: p.bHiddenBefore ?? false,
            hiddenAfter: p.bHiddenAfter ?? false,
            spectacle: p.bSpectacle ?? false,
            moving: Boolean(p.MovingAnimation),
        };
        if (actor.class === "LFFFireworksActor") {
            entry.cue.fireworks = {
                burstOffsets: (p.BurstOffsets ?? []).map(vec),
                burstColours: (p.BurstColors ?? []).map((c) => round([c.r, c.g, c.b], 3)),
                sparksPerBurst: p.SparksPerBurst ?? 60,
                burstRadius: p.BurstRadius ?? 1500,
                burstSeconds: p.BurstSeconds ?? 3.5,
                burstInterval: p.BurstInterval ?? 1.6,
                sparkSize: p.SparkSize ?? 130,
            };
        }
    }
    // Attached actors (the eagle's mesh rides its flight cue) move and hide with their parent.
    const attach = comps.find((c) => c.isRoot)?.attachParent;
    if (attach && attach.actorLabel && attach.actorLabel !== "LFF_SiteRoot") entry.attachedTo = attach.actorLabel;
    if (actor.class === "GeometryCacheActor") entry.eagle = true;
    if (parts.length === 0 && !text && !entry.cue && !entry.boxes && !entry.eagle && !tags.some((t) => t.startsWith("LFF_Place_"))) continue;
    actors.push(entry);
}

// ---- the rest ----
const g = geo.georeference ?? geo.CesiumGeoreference ?? geo;
const origin = {
    originLatitude: g.OriginLatitude ?? g.originLatitude ?? 39.9008036,
    originLongitude: g.OriginLongitude ?? g.originLongitude ?? -75.167479,
    originHeight: g.OriginHeight ?? g.originHeight ?? -31.04,
};
const siteRootActor = props.actors.find((a) => (a.tags ?? []).includes("LFF_SiteRoot"));
const footprint = (geo.cartographicPolygons ?? [])[0]?.pointsLonLatHeight?.map((p) => [p[0], p[1]]) ?? [];

const scene = {
    source: "Content/LFF/Levels/L_LFF_Stadium.umap via web/tools/extract_umap.py + build_scene_data.mjs",
    georeference: {
        ...origin,
        siteRoot: { location: siteRootActor.world.location, rotation: siteRootActor.world.rotation },
    },
    contextTilesetIonAssetId: geo.tilesets?.[0]?.IonAssetID ?? 2275207,
    footprintLonLat: footprint,
    targets: Object.fromEntries(Object.entries(targets).map(([id, t]) => [id, { l: round([t.world.x, t.world.y, t.world.z], 2),
        r: round([t.world.pitch, t.world.yaw, t.world.roll], 5), fov: t.fov }])),
    paths: paths.map((p) => ({ name: p.label, from: p.from ?? "", to: p.to ?? "", travelSeconds: p.travelSeconds ?? 8, cutAtEnd: p.bCutAtEnd ?? false,
        waypoints: p.waypointsWorld.map((w) => round(vec(w), 2)) })),
    labels: labels.map((l) => l.lblTags).flat().length,
    places: Object.fromEntries(places.flatMap((p) => p.placeTags.map((tag) => [tag, round(vec(p.world.location), 2)]))),
    actors,
};
writeFileSync(join(data, "scene.json"), JSON.stringify(scene));
console.log(`scene.json: ${actors.length} actors (${skippedStadium} stadium model meshes left to the ion asset), ${Object.keys(scene.targets).length} targets, ` +
    `${scene.paths.length} paths, ${Object.keys(scene.places).length} places; ${(JSON.stringify(scene).length / 1024).toFixed(0)} KB.`);
