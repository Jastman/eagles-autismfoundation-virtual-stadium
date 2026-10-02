"""Finds every seat in the stadium model's seat chunks, for the crowd (Tools/Editor/finish_pass.py, step "bowl").

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/extract_seats.py -- \
      --chunks Saved/StadiumImport/v1 --out Saved/CrowdImport/seats.json

Each chair in the CGTrader model is its own loose part of about 50 triangles. Every loose part of a SM_LFF_Seats*
chunk (the chunks from Tools/Pipeline/prepare_stadium.py, already in the site frame) that is chair-sized becomes one
seat: its centre on the floor (the part's lowest point) and the direction it faces, toward the nearest point of the
field, as seats in a bowl do. Output is in the Unreal site frame (LFF_SiteRoot local): cm, +X toward the east stand,
+Y toward the south end (the chunks' Blender frame has +Y north), Z up, turf at 0.
"""
import argparse, glob, json, math, os, sys

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("--chunks", required=True)
ap.add_argument("--out", required=True)
a = ap.parse_args(argv)

# The field's bounds in the chunks' frame (manifest.json, SM_LFF_Field), metres: seats face its nearest point.
FIELD_X, FIELD_Y = (-41.95, 37.45), (-59.4, 64.41)
# A chair is 0.3-0.8 m across and 0.4-1.2 m tall; smaller parts are rails and armrests, bigger ones are benches.
SIZE_XY, SIZE_Z = (0.25, 0.9), (0.35, 1.3)


def components(n_verts, edges):
    parent = np.arange(n_verts)

    def find(i):
        root = i
        while parent[root] != root:
            root = parent[root]
        while parent[i] != root:
            parent[i], i = root, parent[i]
        return root
    for u, v in edges:
        ru, rv = find(u), find(v)
        if ru != rv:
            parent[ru] = rv
    return np.array([find(i) for i in range(n_verts)])


seats = []
for f in sorted(glob.glob(os.path.join(a.chunks, "SM_LFF_Seats*.fbx"))):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=f)
    n_parts = 0
    for ob in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
        me = ob.data
        co = np.empty(len(me.vertices) * 3)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        co = (np.array(ob.matrix_world) @ np.c_[co, np.ones(len(co))].T).T[:, :3]
        ed = np.empty(len(me.edges) * 2, dtype=np.int64)
        me.edges.foreach_get("vertices", ed)
        comp = components(len(co), ed.reshape(-1, 2))
        order = np.argsort(comp)
        comp_sorted = comp[order]
        splits = np.flatnonzero(np.diff(comp_sorted)) + 1
        for idx in np.split(order, splits):
            p = co[idx]
            lo, hi = p.min(0), p.max(0)
            size = hi - lo
            n_parts += 1
            if not (SIZE_XY[0] <= max(size[0], size[1]) <= SIZE_XY[1] and SIZE_Z[0] <= size[2] <= SIZE_Z[1]):
                continue
            cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
            fx = min(max(cx, FIELD_X[0]), FIELD_X[1])
            fy = min(max(cy, FIELD_Y[0]), FIELD_Y[1])
            yaw_blender = math.atan2(fy - cy, fx - cx)
            # Blender (+Y north) to Unreal site frame (+Y south): y -> -y, so the yaw flips sign.
            seats.append([round(cx * 100, 1), round(-cy * 100, 1), round(lo[2] * 100, 1), round(-math.degrees(yaw_blender), 1)])
    print(f"{os.path.basename(f)}: {n_parts} parts, {len(seats)} seats so far", flush=True)

os.makedirs(os.path.dirname(a.out), exist_ok=True)
with open(a.out, "w") as fh:
    json.dump({"frame": "LFF_SiteRoot local, cm; [x, y, floor z, yaw deg]", "seats": seats}, fh)
print(f"wrote {len(seats)} seats to {a.out}")
