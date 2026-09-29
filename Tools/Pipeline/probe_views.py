"""Renders the prepared stadium chunks from camera poses given in the Unreal site frame (quick framing checks).

  Blender -b --factory-startup --python Tools/Pipeline/probe_views.py -- --chunks Saved/StadiumImport/v1 \
      --out <dir> --views views.json [--profile x0,y0,x1,y1]

views.json: {"name": [u_cm, v_cm, z_cm, pitch_deg, yaw_deg, fov_deg], ...} in LFF_SiteRoot-local Unreal coordinates
(+X across the field, +Y toward the south end, Z up). The chunks are in Blender's site map frame (+Y north), so
Unreal (u, v) maps to Blender (u, -v) and an Unreal yaw psi to a Blender heading of -psi.
--profile ray-casts straight down every 0.5 m along a line (Blender map frame, metres) and prints hit heights.
"""
import argparse, glob, json, math, os, sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("--chunks", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--views", default=None)
ap.add_argument("--profile", default=None)
ap.add_argument("--skip", default="", help="comma-separated chunk name substrings to leave out")
a = ap.parse_args(argv)
os.makedirs(a.out, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
skip = [s for s in a.skip.split(",") if s]
for f in sorted(glob.glob(os.path.join(a.chunks, "*.fbx"))):
    if any(s in os.path.basename(f) for s in skip):
        continue
    bpy.ops.import_scene.fbx(filepath=f)

if a.profile:
    x0, y0, x1, y1 = map(float, a.profile.split(","))
    deps = bpy.context.evaluated_depsgraph_get()
    n = int(math.hypot(x1 - x0, y1 - y0) / 0.5) + 1
    rows = []
    for i in range(n):
        t = i / max(n - 1, 1)
        p = Vector((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 120.0))
        hit, loc, nrm, idx, ob, _ = bpy.context.scene.ray_cast(deps, p, Vector((0, 0, -1)))
        rows.append((round(p.x, 1), round(p.y, 1), round(loc.z, 2) if hit else None, ob.name if hit else None))
    print("PROFILE", json.dumps(rows))

if a.views:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    cam_data = bpy.data.cameras.new("cam")
    cam_data.sensor_fit = "HORIZONTAL"
    cam_data.clip_start, cam_data.clip_end = 0.05, 3000
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    for name, (u, v, z, pitch, yaw, fov) in json.load(open(a.views)).items():
        cam_data.angle = math.radians(fov)
        cam.location = (u / 100.0, -v / 100.0, z / 100.0)
        heading = -yaw  # Blender heading from +X, counter-clockwise
        cam.rotation_euler = (math.radians(90 + pitch), 0.0, math.radians(heading - 90))
        scene.render.filepath = os.path.join(a.out, f"{name}.png")
        bpy.ops.render.render(write_still=True)
        print("VIEW", name, scene.render.filepath)
