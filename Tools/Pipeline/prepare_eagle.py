"""Bakes the bald eagle glTF (Assets/bald-eagle) into an Alembic geometry cache for Unreal.

The source rig mixes three skins with about 290 rigid feather meshes, some animated on their own nodes, so a
skeletal import would lose parts of the wing flap. Alembic bakes every evaluated vertex instead: one 2 s flap loop
(48 frames at 24 fps), turned so the eagle flies along +X with +Z up and scaled to a real wingspan.

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python Tools/Pipeline/prepare_eagle.py -- \
      --src "Assets/bald-eagle/source/Bald Eagle 05a.gltf" --out Saved/EagleImport

Writes <out>/SK_LFF_BaldEagle.abc, <out>/textures/*.{jpeg,png} and <out>/eagle.json (material -> texture, bounds).
Import with Tools/Editor/import_lff_content.py.
"""
import argparse
import json
import math
import os
import sys

import bpy
import mathutils

WINGSPAN_CM = 280.0  # a little over a real bald eagle (about 2 m), so it reads from the seats


def args():
    p = argparse.ArgumentParser()
    p.add_argument("--src", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args(sys.argv[sys.argv.index("--") + 1:])


def evaluated_bounds(objects):
    dg = bpy.context.evaluated_depsgraph_get()
    lo, hi = mathutils.Vector((1e18,) * 3), mathutils.Vector((-1e18,) * 3)
    for o in objects:
        e = o.evaluated_get(dg)
        for corner in e.bound_box:
            w = o.matrix_world @ mathutils.Vector(corner)
            lo = mathutils.Vector(map(min, lo, w))
            hi = mathutils.Vector(map(max, hi, w))
    return lo, hi


def main():
    a = args()
    os.makedirs(os.path.join(a.out, "textures"), exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(a.src))
    scene = bpy.context.scene
    action = bpy.data.actions[0]
    first, last = (int(round(f)) for f in action.frame_range)
    scene.frame_start, scene.frame_end = first, last - 1  # the last frame repeats the first: a seamless loop
    meshes = [o for o in scene.objects if o.type == "MESH"]
    armature = next(o for o in scene.objects if o.type == "ARMATURE")

    # Which way does it face? The head bone sits forward of the body centre.
    scene.frame_set(first)
    head = next(b for b in armature.pose.bones if "HEAD" in b.name.upper())
    head_w = armature.matrix_world @ head.head
    lo, hi = evaluated_bounds(meshes)
    centre = (lo + hi) / 2
    forward = head_w - centre
    forward.z = 0
    yaw = math.atan2(forward.y, forward.x)

    root = bpy.data.objects.new("LFF_EagleRoot", None)
    scene.collection.objects.link(root)
    for o in list(scene.objects):
        if o.parent is None and o is not root:
            o.parent = root
    root.rotation_euler = (0.0, 0.0, -yaw)  # head toward +X, so the wings span Y
    bpy.context.view_layer.update()

    # The widest wing spread over the loop sets the scale.
    span = 0.0
    for f in range(first, last, 2):
        scene.frame_set(f)
        lo, hi = evaluated_bounds(meshes)
        span = max(span, hi.y - lo.y)
    # Blender metres go out as Alembic metres; Unreal's importer turns them into cm.
    scale = (WINGSPAN_CM / 100.0) / max(span, 1e-6)
    root.scale = (scale, scale, scale)
    bpy.context.view_layer.update()
    scene.frame_set(first)
    lo, hi = evaluated_bounds(meshes)
    root.location = -(lo + hi) / 2
    bpy.context.view_layer.update()
    lo, hi = evaluated_bounds(meshes)

    abc = os.path.join(os.path.abspath(a.out), "SK_LFF_BaldEagle.abc")
    bpy.ops.wm.alembic_export(filepath=abc, start=scene.frame_start, end=scene.frame_end, selected=False,
                              flatten=True, uvs=True, normals=True, face_sets=True, packuv=False,
                              export_hair=False, export_particles=False, evaluation_mode="RENDER")

    materials = {}
    for m in bpy.data.materials:
        tex = None
        if m.use_nodes:
            for n in m.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image is not None:
                    name = f"T_LFF_Eagle_{len(materials)}"
                    ext = "png" if n.image.file_format == "PNG" else "jpeg"
                    path = os.path.join(os.path.abspath(a.out), "textures", f"{name}.{ext}")
                    n.image.filepath_raw = path
                    n.image.file_format = "PNG" if ext == "png" else "JPEG"
                    n.image.save()
                    tex = path
                    break
        base = list(m.diffuse_color[:3])
        materials[m.name] = {"texture": tex, "tint": base, "masked": m.blend_method in ("BLEND", "CLIP", "HASHED")}
    info = {"fps": scene.render.fps, "frames": scene.frame_end - scene.frame_start + 1,
            "loop_seconds": (scene.frame_end - scene.frame_start + 1) / scene.render.fps,
            "wingspan_cm": WINGSPAN_CM, "bounds_m": [list(lo), list(hi)], "yaw_applied_deg": math.degrees(-yaw),
            "materials": materials}
    with open(os.path.join(a.out, "eagle.json"), "w") as f:
        json.dump(info, f, indent=2)
    print(f"[LFF eagle] wrote {abc}: {info['frames']} frames, scale {scale:.4f}, bounds {info['bounds_m']}")


main()
