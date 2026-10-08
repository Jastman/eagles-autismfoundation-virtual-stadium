"""Trace actual stadium treads beneath each fan; never treat a chair's underside as floor.

Blender background:
  Blender -b --factory-startup --python Tools/Pipeline/build_seat_treads.py -- \
    --chunks Saved/StadiumImport/v1 --seats Content/LFF/Crowd/seats.json \
    --out Content/LFF/Crowd/seat_treads.json

The sidecar is tied to the exact seat file and records separate standing/seated foot positions.
Missing treads are null, not guessed heights. Runtime reports and excludes those seats.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import numpy as np

STANDING_OFFSET = 70.0
SEATED_OFFSET = 45.0
SEATED_LATERAL_OFFSET = 13.0
MAX_DROP = 1.5


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--chunks", type=Path, required=True)
    ap.add_argument("--seats", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args(sys.argv[sys.argv.index("--") + 1:])
    raw = args.seats.read_bytes()
    seats = json.loads(raw)["seats"]
    chunks = sorted(args.chunks.glob("SM_LFF_Structure*.fbx"))
    if not chunks or not seats:
        raise RuntimeError("Seat list or stadium structure chunks are missing")
    points = []
    for x, y, z, yaw in seats:
        angle = math.radians(yaw)
        samples = []
        for offset, lateral in ((STANDING_OFFSET, 0.0), (SEATED_OFFSET, SEATED_LATERAL_OFFSET)):
            samples.append(Vector(((x + math.cos(angle) * offset - math.sin(angle) * lateral) / 100,
                                   -(y + math.sin(angle) * offset + math.cos(angle) * lateral) / 100, z / 100 - 0.001)))
        points.append(samples)
    heights = [[None, None] for _ in seats]
    for chunk in chunks:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(chunk.resolve()))
        for ob in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
            me = ob.data
            co = np.empty(len(me.vertices) * 3)
            me.vertices.foreach_get("co", co)
            co = (np.array(ob.matrix_world) @ np.c_[co.reshape(-1, 3), np.ones(len(me.vertices))].T).T[:, :3]
            lo, hi = co.min(0), co.max(0)
            tree = BVHTree.FromPolygons(co.tolist(), [tuple(p.vertices) for p in me.polygons])
            for index, samples in enumerate(points):
                for kind, point in enumerate(samples):
                    if not (lo[0] <= point.x <= hi[0] and lo[1] <= point.y <= hi[1] and point.z >= lo[2]):
                        continue
                    start = point.copy()
                    for _ in range(6):
                        distance = MAX_DROP - (point.z - start.z)
                        if distance <= 0:
                            break
                        hit, normal, _, _ = tree.ray_cast(start, Vector((0, 0, -1)), distance)
                        if hit is None:
                            break
                        if abs(normal.z) >= 0.9:
                            z = round(hit.z * 100, 2)
                            if heights[index][kind] is None or z > heights[index][kind]:
                                heights[index][kind] = z
                            break
                        start.z = hit.z - 0.001
        print(f"traced {chunk.name}", flush=True)
    valid = sum(all(z is not None for z in row) for row in heights)
    if valid < len(seats) * 0.95:
        raise RuntimeError(f"Insufficient measured tread coverage: {valid}/{len(seats)}; no sidecar written")
    near = sorted(range(len(seats)), key=lambda i: (seats[i][0] + 5050)**2 + seats[i][1]**2)[:8]
    print("Near My seat [chair underside, standing floor, seated floor]:",
          [(i, seats[i][2], heights[i]) for i in near], flush=True)
    result = {"version": 1, "seats_sha256": hashlib.sha256(raw).hexdigest(),
              "seats_sha1": hashlib.sha1(raw).hexdigest(),
              "standing_offset_cm": STANDING_OFFSET, "seated_offset_cm": SEATED_OFFSET,
              "seated_lateral_offset_cm": SEATED_LATERAL_OFFSET,
              "tread_z_cm": heights}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, separators=(",", ":")) + "\n")
    print(f"Measured {valid}/{len(seats)} treads; wrote {args.out}", flush=True)


main()
