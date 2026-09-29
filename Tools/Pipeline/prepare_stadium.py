"""Prepares the CGTrader Lincoln Financial Field model for Unreal (Docs/ASSET_INGESTION.md).

Headless Blender 5.x:
  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/prepare_stadium.py -- \
      --src "Assets/Eagles Stadium Philadelphia 3D Model/OBJ File format (Exchange)" \
      --tile-dsm Saved/StadiumImport/tile_dsm.json --out Saved/StadiumImport/v1

What it does:
  1. Imports the stadium, the NFL field (the soccer field is not used) and the five seat parts as OBJ. OBJ is the
     source because it's the only delivered format whose materials reference the textures (the FBX has neither
     embedded media nor texture paths). Units are metres, Y-up; Blender converts to Z-up.
  2. Classifies every object (the source has ~41k generically named objects) as FIELD, SEATS, GROUND or STRUCTURE.
  3. Joins them into ~30 chunks: 45 degree sectors x inner/outer ring. One giant mesh gives Lumen poor distance fields
     and surface cache coverage; 41k meshes means 41k draw calls. Chunks keep Nanite culling and Lumen quality good.
  4. Registers the model to the Google tiles by matching a height grid (tile_dsm.json, sampled in the site frame by
     line traces) against the model's own heights over a rotation/offset search, and bakes the best fit into the
     geometry. Output is in the site frame: X across the field toward the east stand, Y toward the NORTH end (Blender
     is right-handed; Unreal's FBX import flips Y, which yields the site's +Y = south), Z up, turf at 0.
  5. Exports one FBX per chunk (geometry and material slot names only) plus manifest.json with the registration,
     triangle counts and the tile clip polygon.

Never writes into Assets/ (the licensed source stays untouched) or Content/.
"""
import argparse, collections, json, math, os, sys, time

import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--src", required=True)
ap.add_argument("--tile-dsm", default=None)
ap.add_argument("--out", required=True)
ap.add_argument("--no-seats", action="store_true", help="skip the 3.7M-triangle seat parts (for quick tests)")
args = ap.parse_args(argv)
os.makedirs(args.out, exist_ok=True)

STADIUM = "Eagles Stadium Philadelphia 3D Model.obj"
FIELD = "Football Field NFL.obj"
SEATS = [f"3D seats/3D Seats Part{i}.obj" for i in range(1, 6)]
GROUND_MATERIALS = {"Asphalt", "Tiles1", "BaseFloor"}
SECTORS = 8
INNER_RADIUS_M = 110.0
T0 = time.time()


def log(msg):
    print(f"[prepare_stadium {time.time() - T0:6.0f}s] {msg}", flush=True)


def import_obj(rel, layer):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=os.path.join(args.src, rel), forward_axis="NEGATIVE_Z", up_axis="Y",
                          use_split_objects=True, use_split_groups=True)
    new = [o for o in set(bpy.data.objects) - before if o.type == "MESH"]
    for o in new:
        o["lff_layer"] = layer
    log(f"imported {rel}: {len(new)} objects")
    return new


def tri_count(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def dominant_material(o):
    counts = collections.Counter()
    for p in o.data.polygons:
        slot = o.material_slots[p.material_index] if p.material_index < len(o.material_slots) else None
        counts[slot.material.name if slot and slot.material else ""] += 1
    return counts.most_common(1)[0][0] if counts else ""


def world_bounds(o):
    # From the vertices, not o.bound_box: the cached box isn't refreshed after mesh.transform() in background mode.
    co = np.empty(len(o.data.vertices) * 3, np.float64)
    o.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ np.array(o.matrix_world.to_3x3()).T + np.array(o.matrix_world.translation)
    return Vector(co.min(axis=0)), Vector(co.max(axis=0))


def bake_transforms(objs):
    """Applies each object's transform (the importer puts the Y-up conversion there) to its mesh data."""
    for o in objs:
        o.data.transform(o.matrix_world)
        o.matrix_world = Matrix.Identity(4)


# ---------------------------------------------------------------- 1. import
bpy.ops.wm.read_factory_settings(use_empty=True)
objs = import_obj(STADIUM, "STADIUM") + import_obj(FIELD, "FIELD")
if not args.no_seats:
    for s in SEATS:
        objs += import_obj(s, "SEATS")
bake_transforms(objs)

# ---------------------------------------------------------------- 2. classify
for o in objs:
    if o["lff_layer"] == "STADIUM":
        lo, hi = world_bounds(o)
        m = dominant_material(o)
        o["lff_layer"] = "GROUND" if (m in GROUND_MATERIALS or (m == "Grass" and hi.z < 3.0)) else "STRUCTURE"
by_layer = collections.Counter(o["lff_layer"] for o in objs)
log(f"classified: {dict(by_layer)}")

# ---------------------------------------------------------------- 4. registration (before chunking; uses raw objects)
registration = {"applied": False}
if args.tile_dsm:
    dsm = json.load(open(args.tile_dsm))
    step, half = dsm["step_m"], dsm["half"]
    grid = np.array([[np.nan if z is None else z for z in row] for row in dsm["grid_v_rows_u_cols"]], dtype=np.float64)
    # Site sample positions in a right-handed map frame: e = u (across field), n = -v (Unreal +Y is south).
    idx = np.arange(-half, half + 1) * step
    U, V = np.meshgrid(idx, idx)            # rows = v, cols = u
    E, N = U, -V
    valid = ~np.isnan(grid)
    e_s, n_s, z_s = E[valid], N[valid], grid[valid]

    # Model height field (structure + ground, not seats/field) on a 1 m grid by downward ray casts.
    verts, polys = [], []
    for o in objs:
        if o["lff_layer"] in ("STRUCTURE", "GROUND", "FIELD"):
            base = len(verts)
            verts.extend(v.co.copy() for v in o.data.vertices)
            polys.extend([base + i for i in p.vertices] for p in o.data.polygons)
    bvh = BVHTree.FromPolygons(verts, polys, all_triangles=False)
    R = 200
    xs = np.arange(-R, R + 1, 1.0)
    model = np.full((len(xs), len(xs)), np.nan)
    down = Vector((0, 0, -1))
    for j, y in enumerate(xs):
        for i, x in enumerate(xs):
            hit = bvh.ray_cast(Vector((x, y, 200.0)), down, 400.0)
            if hit[0] is not None:
                model[j, i] = hit[0].z
    log("model height field sampled")

    def score(theta_deg, te, tn):
        t = math.radians(theta_deg)
        c, s = math.cos(t), math.sin(t)
        # Model point = R(-theta) * (site - t)
        dx, dy = e_s - te, n_s - tn
        mx, my = c * dx + s * dy, -s * dx + c * dy
        ii, jj = np.rint(mx + R).astype(int), np.rint(my + R).astype(int)
        ok = (ii >= 0) & (ii < len(xs)) & (jj >= 0) & (jj < len(xs))
        zm = np.full(len(mx), np.nan)
        zm[ok] = model[jj[ok], ii[ok]]
        both = ~np.isnan(zm)
        if both.sum() < 500:
            return 1e9, 0.0, 0
        d = z_s[both] - zm[both]
        dz = float(np.median(d))
        return float(np.median(np.abs(d - dz))), dz, int(both.sum())

    best = None
    coarse = []
    for base in (0, 90, 180, 270):
        for dt in np.arange(-12, 12.01, 2.0):
            for te in np.arange(-8, 8.01, 2.0):
                for tn in np.arange(-8, 8.01, 2.0):
                    sc, dz, n = score(base + dt, te, tn)
                    coarse.append((sc, base + dt, te, tn, dz, n))
    coarse.sort()
    by_base = {}
    for c_ in coarse:
        by_base.setdefault(int(round(c_[1] / 90.0)) % 4 * 90, c_)
    s0, th0, te0, tn0, _, _ = coarse[0]
    for dt in np.arange(-2, 2.01, 0.25):
        for te in np.arange(-2, 2.01, 0.25):
            for tn in np.arange(-2, 2.01, 0.25):
                sc, dz, n = score(th0 + dt, te0 + te, tn0 + tn)
                if best is None or sc < best[0]:
                    best = (sc, th0 + dt, te0 + te, tn0 + tn, dz, n)
    sc, theta, te, tn, dz, n = best

    def elevated_fit(theta_deg, te_, tn_):
        """Share of elevated tile samples (stands/roof, > 8 m) the model matches within 2 m: discriminates the
        near-symmetric 0/180 degree cases far better than the all-points median."""
        t = math.radians(theta_deg); c, s = math.cos(t), math.sin(t)
        sel = z_s > 8.0
        dx, dy = e_s[sel] - te_, n_s[sel] - tn_
        mx, my = c * dx + s * dy, -s * dx + c * dy
        ii, jj = np.rint(mx + R).astype(int), np.rint(my + R).astype(int)
        ok = (ii >= 0) & (ii < len(xs)) & (jj >= 0) & (jj < len(xs))
        zm = np.full(len(mx), np.nan); zm[ok] = model[jj[ok], ii[ok]]
        good = ~np.isnan(zm)
        return round(float(np.mean(np.abs(z_s[sel][good] - zm[good]) < 2.0)), 3), int(sel.sum())

    alt = min((c_ for c_ in coarse if abs(((c_[1] - theta + 180) % 360) - 180) > 90), key=lambda c_: c_[0])
    registration = {
        "applied": True, "theta_deg": theta, "offset_e_m": te, "offset_n_m": tn, "tile_minus_model_dz_m": dz,
        "median_abs_residual_m": sc, "samples": n,
        "best_score_by_base_orientation": {str(k): round(v[0], 3) for k, v in sorted(by_base.items())},
        "elevated_within_2m_best": elevated_fit(theta, te, tn),
        "elevated_within_2m_flipped_180": elevated_fit(alt[1], alt[2], alt[3]),
        "note": "theta rotates the model counter-clockwise (from above) into the site map frame; heights are not "
                "shifted (the model turf defines the site turf at Z=0; dz is informational).",
    }
    log(f"registration: {registration}")
    M = Matrix.Translation((te, tn, 0.0)) @ Matrix.Rotation(math.radians(theta), 4, "Z")
    for o in objs:
        o.data.transform(M)

# ---------------------------------------------------------------- 3. chunk
def chunk_key(o):
    layer = o["lff_layer"]
    if layer in ("FIELD", "GROUND"):
        return layer
    lo, hi = world_bounds(o)
    c = (lo + hi) / 2
    sector = int(((math.degrees(math.atan2(c.y, c.x)) + 360.0) % 360.0) // (360.0 / SECTORS))
    ring = "In" if math.hypot(c.x, c.y) < INNER_RADIUS_M else "Out"
    return f"{layer}_S{sector}{'_' + ring if layer == 'STRUCTURE' else ''}"


def merge(name, members):
    """Merges meshes with bulk array copies (bpy.ops.object.join stalls on tens of thousands of objects).
    Keeps positions, faces, the first UV map, per-corner normals and material slots."""
    mats, mat_index = [], {}
    vcos, loops_v, poly_start, poly_total, poly_mat, uvs, normals = [], [], [], [], [], [], []
    v_off = l_off = 0
    for o in members:
        me = o.data
        nv, nl, np_ = len(me.vertices), len(me.loops), len(me.polygons)
        if np_ == 0:
            continue
        co = np.empty(nv * 3, np.float32); me.vertices.foreach_get("co", co)
        lv = np.empty(nl, np.int32); me.loops.foreach_get("vertex_index", lv)
        ls = np.empty(np_, np.int32); me.polygons.foreach_get("loop_start", ls)
        lt = np.empty(np_, np.int32); me.polygons.foreach_get("loop_total", lt)
        pm = np.empty(np_, np.int32); me.polygons.foreach_get("material_index", pm)
        remap = []
        for s in o.material_slots:
            key = s.material.name if s.material else ""
            if key not in mat_index:
                mat_index[key] = len(mats); mats.append(s.material)
            remap.append(mat_index[key])
        pm = np.array(remap, np.int32)[np.clip(pm, 0, max(len(remap) - 1, 0))] if remap else np.zeros(np_, np.int32)
        uv = np.zeros(nl * 2, np.float32)
        if me.uv_layers:
            me.uv_layers[0].data.foreach_get("uv", uv)
        cn = np.empty(nl * 3, np.float32); me.corner_normals.foreach_get("vector", cn)
        vcos.append(co); loops_v.append(lv + v_off); poly_start.append(ls + l_off); poly_total.append(lt)
        poly_mat.append(pm); uvs.append(uv); normals.append(cn)
        v_off += nv; l_off += nl
    me = bpy.data.meshes.new(name)
    co, lv = np.concatenate(vcos), np.concatenate(loops_v)
    ls, lt, pm = np.concatenate(poly_start), np.concatenate(poly_total), np.concatenate(poly_mat)
    me.vertices.add(len(co) // 3); me.vertices.foreach_set("co", co)
    me.loops.add(len(lv)); me.loops.foreach_set("vertex_index", lv)
    me.polygons.add(len(ls)); me.polygons.foreach_set("loop_start", ls); me.polygons.foreach_set("loop_total", lt)
    me.polygons.foreach_set("material_index", pm)
    uv_layer = me.uv_layers.new(name="UVMap"); uv_layer.data.foreach_set("uv", np.concatenate(uvs))
    for m in mats:
        me.materials.append(m)
    me.update(calc_edges=True)
    me.normals_split_custom_set(np.concatenate(normals).reshape(-1, 3).tolist())
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


groups = collections.defaultdict(list)
for o in objs:
    groups[chunk_key(o)].append(o)
chunks = {}
for key, members in sorted(groups.items()):
    name = "SM_LFF_" + "".join(p.capitalize() for p in key.lower().split("_"))
    chunks[name] = merge(name, members)
    log(f"merged {name}: {len(members)} objects")
bpy.data.batch_remove(objs)  # drop the source objects so the exports and memory only carry the chunks
log(f"{len(chunks)} chunks")

# ---------------------------------------------------------------- 5. clip polygon (ground outline, inset 1 m)
def convex_hull(points):
    pts = sorted(set(points))
    cross = lambda o, a, b: (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


ground = chunks.get("SM_LFF_Ground")
hull = convex_hull([(round(v.co.x, 2), round(v.co.y, 2)) for v in ground.data.vertices]) if ground else []
cx = sum(p[0] for p in hull) / len(hull) if hull else 0.0
cy = sum(p[1] for p in hull) / len(hull) if hull else 0.0
inset = []
for x, y in hull:  # pull each point 1 m toward the centroid so the model ground overlaps the tiles at the seam
    d = math.hypot(x - cx, y - cy) or 1.0
    inset.append((round(x - (x - cx) / d, 2), round(y - (y - cy) / d, 2)))

# ---------------------------------------------------------------- export
manifest = {"source": os.path.abspath(args.src), "registration": registration, "chunks": {},
            "frame": "site map frame, metres: X across field (east stand), Y toward north end, Z up, turf 0",
            "clip_polygon_xy_m": inset}
for name, o in chunks.items():
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    path = os.path.join(args.out, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", use_mesh_modifiers=False, mesh_smooth_type="FACE",
                             add_leaf_bones=False, bake_anim=False, path_mode="STRIP", embed_textures=False)
    lo, hi = world_bounds(o)
    manifest["chunks"][name] = {"triangles": tri_count(o), "materials": sorted({s.material.name for s in o.material_slots if s.material}),
                                "bounds_min_m": [round(c, 2) for c in lo], "bounds_max_m": [round(c, 2) for c in hi],
                                "file": os.path.basename(path)}
    log(f"exported {name}: {manifest['chunks'][name]['triangles']} tris")
manifest["total_triangles"] = sum(c["triangles"] for c in manifest["chunks"].values())
json.dump(manifest, open(os.path.join(args.out, "manifest.json"), "w"), indent=1)
log(f"DONE: {len(chunks)} chunks, {manifest['total_triangles']} triangles")
