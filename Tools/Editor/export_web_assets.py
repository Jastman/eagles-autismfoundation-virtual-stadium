"""Exports the Unreal-only art the web build draws as stand-ins (Docs/WEB_PARITY.md W5, W6, G1) into web/assets-src/
(git-ignored; web/tools/prepare_assets.mjs serves what it finds there): the props, cars and people as GLB with baked
materials, the people's animations, and the 30 phone ticket frames as full-resolution PNG. Read-only: it loads assets
(and opens L_LFF_Stadium if another level without unsaved changes is open) and never modifies or saves either.

UNTESTED. Written against the UE 5.8 Python and glTF Exporter APIs without an editor to run it in. Every exporter
option is set through a guard that logs (and records in the report) a name the engine doesn't know instead of
failing, so read the first run's "[LFF web export]" warnings and export_report.json "optionWarnings".

Output (web/assets-src/)
  models/<AssetName>.glb           one per static or skeletal mesh, named by the asset name only: SM_LFF_Barrier.glb,
                                   SM_vehCar_vehicle05_No_Wheel.glb, SM_Wheel_Front_L_vehCar_vehicle05.glb,
                                   SKM_MHC_AverageMale_Head01_BodyMesh.glb, SKM_LFF_AverageMale_Head01_Hair_S_Clean.glb.
                                   Metres (exporter scale 0.01). Skeletal meshes carry their skeleton and skin weights.
  models/<AssetName>.bounds.json   {"asset": "/Game/...", "min": [x, y, z], "max": [x, y, z], "units": "cm",
                                   "frame": "unreal-local"}: the asset's own box, read from the asset, in Unreal's axes
                                   (X forward, Y right, Z up), so the web can tell how the exporter turned the mesh.
                                   MetaHuman meshes face +Y (ALFFPersonActor turns them -90 deg yaw).
  models/anim/<AnimName>.glb       an animation: its preview mesh (skeleton + skin, no materials) and the clip. The
                                   crowd and walk clips play on SKEL_MetaHumanCrowd (SKM_MetaHumanCrowd, the hidden
                                   "Pose" mesh); bodies, faces and hair copy its pose by bone name. AS_LFF_StaffSmile
                                   is a facial clip on the face skeleton. (+ .bounds.json of the preview mesh)
  models/people/<ActorLabel>.glb   each ALFFPersonActor in the level, as placed: body, face and hair in one GLB (the
                                   actor itself is exported, nothing is spawned). The GLB root carries the actor's world
                                   transform; its .bounds.json is "frame": "unreal-world" with "actorTransform".
                                   The outfits are Chaos cloth assets, which the glTF exporter can't write: the people
                                   come out without tee, jeans and shoes (export_report.json lists each outfit and the
                                   tee colour). The bowl's detailed fans (spawner DetailedLooks) are not actors; they're
                                   listed as "looks" (Look0..N: body, face, hair, outfit) built from the GLBs above.
  ticket/NN.png                    T_LFF_Ticket_00..29 in frame order, full resolution (the texture's source image;
                                   falls back to Saved/TicketImport/T_LFF_Ticket_NN.png if the export fails).
  export_report.json               exported, up to date, skipped (with the reason), removed; bytes, LOD, triangles and
                                   embedded image sizes per GLB; the looks; licence notices.
  .export_state.json               what each output was made from, for the up-to-date check.

What it exports
  By default: the project's own props (/Game/LFF/..., except the stadium) and the ticket frames.
  --include-city-sample       City Sample Vehicles (/Game/CitySampleVehicles): body, wheels, glass of the 5 cars.
  --include-metahuman-crowd   MetaHuman Crowd sample bodies, faces and pose mesh (/MetaHumanCrowdContent), the fitted
                              hair cards (/Game/LFF/People/Hair), every clip the people play (/Game/LFF/Crowd/AS_*,
                              /Game/LFF/People/AS_*, /MetaHumanCrowdContent/Animations/AS_*) and models/people/.
                              The hair and the LFF clips are made from the sample's meshes and skeleton, so they
                              share its licence.
  --include-stadium           /Game/LFF/Stadium meshes (the licensed CGTrader model; the web streams the stadium from
                              Cesium ion, so this is for local comparison only, and large).
  Assets come from L_LFF_Stadium's references (asset registry), the person actors and crowd spawner in the level, and
  a fixed list of the assets the level is known to use (KNOWN_ASSETS). Engine assets (/Engine/BasicShapes) are left
  out. The eagle (GC_LFF_BaldEagle) is a geometry cache, which the glTF exporter can't write: export eagle.glb from
  Assets/bald-eagle in Blender instead (WEB_PARITY G2).

LICENCES: City Sample Vehicles and the MetaHuman Crowd sample may be licensed for use in Unreal Engine only. Check
your licence before putting their GLBs on the web. Both are off unless asked for; when on, a reminder is logged and
recorded in the report. Outputs this script wrote for a group that is now off are deleted on the next run (only
files listed in .export_state.json), so a licensed GLB can't linger in assets-src/ and ship by accident.

Options
  --force                 re-export everything (otherwise a file is skipped while its source asset file, the level
                          for people/, and the export settings are unchanged; a material or texture edit alone
                          doesn't count: use --force)
  --texture-size N        baked material texture size (default 1024). Textures the exporter passes through unbaked
                          (a material input that is a plain texture sample) keep their own size; the report lists
                          every embedded image larger than N ("oversizeImages"). Shrink those with, for example,
                          npx @gltf-transform/cli resize in.glb out.glb --width 1024 --height 1024
  --lod auto|N            mesh LOD (default auto: the most detailed LOD with at most --max-triangles triangles;
                          skeletal meshes, whose triangle count Python can't read, use vertices <= max-triangles / 1.7)
  --max-triangles N       auto LOD budget (default 15000: MetaHumans land around 5-15k triangles)
  --bake mesh|simple|off  material baking (default mesh: bake base colour, normal, ORM using the mesh's UVs).
                          Baking renders, so it needs a GPU: under -nullrhi (run_headless.sh) it is switched off and
                          the report says so. Run in the open editor for baked materials.
  --jpeg                  JPEG for colour textures (normal maps stay PNG); default PNG
  --only PART [PART ...]  models, anims, people, ticket (default: all)
  --dry-run               list what would be exported, with the LOD each would use; writes only the report
  --quit                  quit the editor when done (for a direct UnrealEditor -ExecutePythonScript run)

How to run (on the workstation; the project path has spaces, so headless options go in LFF_WEB_EXPORT_ARGS)
  Open editor, L_LFF_Stadium loaded, PIE stopped (recommended: materials bake), in the Output Log's Cmd console:
    py "<repo>/Tools/Editor/export_web_assets.py" --include-city-sample --include-metahuman-crowd
  or through Unreal MCP (execute_python_code):
    import os, runpy, sys, unreal
    os.environ["LFF_WEB_EXPORT_ARGS"] = "--include-city-sample"
    runpy.run_path(unreal.Paths.project_dir() + "Tools/Editor/export_web_assets.py", run_name="__main__")
  Headless, editor closed (geometry and ticket frames only: run_headless.sh passes -nullrhi, so nothing is baked):
    LFF_WEB_EXPORT_ARGS="--include-city-sample" Tools/Editor/run_headless.sh Tools/Editor/export_web_assets.py "LFF web export"
  Headless with baking: the same UnrealEditor command run_headless.sh uses, without -nullrhi, and --quit:
    LFF_WEB_EXPORT_ARGS="--quit" "$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
      "$PWD/VirtualStadium.uproject" -ExecutePythonScript="$PWD/Tools/Editor/export_web_assets.py" \
      -ModelContextProtocolPort=18000 -unattended -nopause -nosplash -log
  Then: cd web && npm run prepare-assets. Needs the "glTF Exporter" engine plugin (Edit > Plugins); the script stops
  with a clear message if it is off. Logs "[LFF web export] DONE" or "FAILED".

Watch on the first run
  unreal.GLTFExporter.export_to_gltf (return shape), the GLTFExportOptions field names (bake_material_inputs,
  default_material_bake_size, default_level_of_detail, export_animation_sequences, export_preview_mesh,
  export_vertex_skin_weights, export_hidden_in_game), whether people/ includes the hidden Pose mesh, LOD/triangle
  readers (StaticMesh.get_num_triangles, SkeletalMeshEditorSubsystem.get_num_verts), and Nanite cars, which may come
  out as their Nanite fallback mesh (flagged "nanite" in the report).
"""
import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import struct
import sys
import time

import unreal

TAG = "LFF web export"
SCRIPT_VERSION = 1  # bump to make every output stale
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJECT_DIR, "web", "assets-src")
MODELS, TICKET_OUT = os.path.join(OUT, "models"), os.path.join(OUT, "ticket")
REPORT, STATE = os.path.join(OUT, "export_report.json"), os.path.join(OUT, ".export_state.json")
PARTIAL = os.path.join(OUT, ".partial")  # exports land here first, so a failed one never reaches models/
LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
LEVEL_FILE = os.path.join(PROJECT_DIR, "Content", "LFF", "Levels", "L_LFF_Stadium.umap")
TICKET_DIR = "/Game/LFF/UI/Ticket"
TICKET_FRAMES = 30
TICKET_FALLBACK = os.path.join(PROJECT_DIR, "Saved", "TicketImport")  # Tools/Pipeline/prepare_ticket.py
GLB_SCALE = 0.01                       # cm -> m, the exporter's default
TRIS_PER_VERT = 1.7                    # skeletal auto LOD: vertices stand in for triangles
POSE_MESH = "/MetaHumanCrowdContent/Animations/SKM_MetaHumanCrowd"  # ALFFPersonActor's hidden "Pose" mesh

# Licence groups, first match wins. "lff" is always on; the others need their flag.
GROUPS = (("stadium", ("/Game/LFF/Stadium/",)),
          ("metahuman", ("/MetaHumanCrowdContent/", "/Game/LFF/People/", "/Game/LFF/Crowd/")),
          ("city", ("/Game/CitySampleVehicles/",)),
          ("lff", ("/Game/LFF/",)))
FLAGS = {"city": "--include-city-sample", "metahuman": "--include-metahuman-crowd", "stadium": "--include-stadium"}
LICENCE = {
    "city": "City Sample Vehicles (Fab/Epic sample content) may be licensed for use in Unreal Engine only. Check your "
            "licence before distributing these GLBs on the web.",
    "metahuman": "MetaHuman Crowd sample content (bodies, faces, pose mesh, clips) and the hair cards and clips made "
                 "from it may be licensed for use in Unreal Engine only. Check your licence before distributing these "
                 "GLBs on the web.",
    "stadium": "The stadium meshes come from the licensed CGTrader model (Assets/, never committed). For local "
               "comparison only; the web streams the stadium from Cesium ion.",
}
EXPORTABLE = {"StaticMesh": "static-mesh", "SkeletalMesh": "skeletal-mesh", "AnimSequence": "animation"}
UNSUPPORTED = {
    "GeometryCache": "geometry cache: the glTF exporter can't write one. Export eagle.glb from Assets/bald-eagle in "
                     "Blender instead (WEB_PARITY G2)",
    "ChaosClothAsset": "Chaos cloth asset: the glTF exporter has no converter for cloth; the web draws the outfit",
}

# What the level is known to use (web/data/level_extract.json, props.json), in case the registry misses any.
CITY_CARS = ("02", "03", "05", "06", "07")  # Tools/Editor/build_lff_standins.py CITY_CARS
MH_HEADS = ("AverageMale_Head01", "AverageFemale_Head01", "AverageMale_Head02", "AverageFemale_Head02")
KNOWN_ASSETS = (
    ["/Game/LFF/Props/Barrier/SM_LFF_Barrier", "/Game/LFF/Props/MetalDetector/SM_LFF_MetalDetector",
     "/Game/LFF/Props/Bag/SM_LFF_Bag", "/Game/LFF/Props/Concession/SM_LFF_Concession",
     "/Game/LFF/Props/SensoryVehicle/SM_LFF_SensoryVehicle",
     "/Game/LFF/Props/SensoryRoom/SM_LFF_SensoryBeanBag/StaticMeshes/SM_LFF_SensoryBeanBag",
     "/Game/LFF/Eagle/GC_LFF_BaldEagle"]
    + [f"/Game/CitySampleVehicles/vehicle{m}_Car/Mesh/{n}" for m in CITY_CARS for n in (
        f"SM_vehCar_vehicle{m}_No_Wheel", f"SM_All_Trans_vehCar_vehicle{m}",
        *(f"SM_Wheel_{w}_vehCar_vehicle{m}" for w in ("Front_L", "Front_R", "Rear_L", "Rear_R")))]
    + [f"/MetaHumanCrowdContent/Characters/Assembled/{h}/{part}/SKM_MHC_{h}_{part}Mesh"
       for h in MH_HEADS for part in ("Body", "Face")]
    + [POSE_MESH, "/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Stand_Idle_Loop",
       "/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Walk_Loop_F"]
    + [f"/Game/LFF/Crowd/AS_LFF_{c}" for c in ("SitClap", "SitIdle", "StandCheer", "StandClap")]
    + ["/Game/LFF/People/AS_LFF_StaffSmile", "/Game/LFF/People/AS_LFF_StaffWave"])
KNOWN_FOLDERS = ("/Game/LFF/People/Hair",)  # every skeletal mesh in these, too
PERSON_PROPS = ("body_mesh", "face_mesh", "pose_mesh", "still_animation", "moving_animation", "facial_animation")
SPAWNER_CLIPS = ("seated_idle", "seated_clap", "standing_idle", "standing_cheer", "standing_clap")
TEE_COLOUR_PARAM = "diffuse_color_1"   # build_lff_standins.py people_materials()

_option_warnings = set()


def log(msg):
    unreal.log(f"[{TAG}] {msg}")


def warn(msg):
    unreal.log_warning(f"[{TAG}] {msg}")


# --- arguments ---------------------------------------------------------------------------------------------------------
def parse_args():
    argv = shlex.split(os.environ.get("LFF_WEB_EXPORT_ARGS", "")) + list(getattr(sys, "argv", [])[1:])
    p = argparse.ArgumentParser(prog="export_web_assets.py", add_help=False)
    p.add_argument("--include-city-sample", action="store_true")
    p.add_argument("--include-metahuman-crowd", action="store_true")
    p.add_argument("--include-stadium", action="store_true")
    p.add_argument("--force", action="store_true")
    p.add_argument("--texture-size", type=int, default=1024)
    p.add_argument("--lod", default="auto")
    p.add_argument("--max-triangles", type=int, default=15000)
    p.add_argument("--bake", choices=("mesh", "simple", "off"), default="mesh")
    p.add_argument("--jpeg", action="store_true")
    p.add_argument("--only", nargs="+", choices=("models", "anims", "people", "ticket"),
                   default=["models", "anims", "people", "ticket"])
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--quit", action="store_true")
    args, unknown = p.parse_known_args(argv)
    if unknown:
        warn(f"ignored arguments: {unknown}")
    if args.lod != "auto" and not args.lod.isdigit():
        raise ValueError(f"--lod must be auto or a number, not {args.lod!r}")
    args.groups = {"lff"} | {g for g, flag in FLAGS.items() if getattr(args, flag[2:].replace("-", "_"))}
    return args


def nullrhi():
    try:
        return "-nullrhi" in unreal.SystemLibrary.get_command_line().lower()
    except Exception:  # noqa: BLE001
        return False


# --- small helpers -----------------------------------------------------------------------------------------------------
def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:  # noqa: BLE001 - missing on this engine version or class
        return default


def package_path(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def group_of(path):
    return next((g for g, prefixes in GROUPS if path.startswith(prefixes)), None)


def asset_class(path):
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    if not data or not data.is_valid():
        return None
    try:
        return str(data.asset_class_path.asset_name)
    except AttributeError:
        return str(data.asset_class)


def source_mtime(obj):
    try:
        return round(os.path.getmtime(unreal.SystemLibrary.get_system_path(obj)), 3)
    except Exception:  # noqa: BLE001 - unknown file: up to date while the output exists
        return None


def write_json(path, data):
    """Writes only when the content changes, so reruns leave unchanged sidecars alone."""
    text = json.dumps(data, indent=2) + "\n"
    if os.path.isfile(path):
        with open(path) as f:
            if f.read() == text:
                return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def rel(path):
    return os.path.relpath(path, OUT).replace(os.sep, "/")


def vec(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


# --- discovery ---------------------------------------------------------------------------------------------------------
def level_references():
    """Packages L_LFF_Stadium references (hard and soft), plus one more hop through its data assets."""
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        ar.wait_for_completion()
    except Exception:  # noqa: BLE001
        pass
    opts = unreal.AssetRegistryDependencyOptions()
    for name, value in (("include_hard_package_references", True), ("include_soft_package_references", True),
                        ("include_searchable_names", False), ("include_hard_management_references", False),
                        ("include_soft_management_references", False)):
        try:
            opts.set_editor_property(name, value)
        except Exception as e:  # noqa: BLE001
            warn(f"asset registry option {name}: {e}")
    found, queue, hops = set(), [LEVEL], {LEVEL: 0}
    while queue:
        package = queue.pop()
        for dep in ar.get_dependencies(package, opts) or []:
            dep = str(dep)
            if dep in hops:
                continue
            hops[dep] = hops[package] + 1
            found.add(dep)
            cls = asset_class(dep) or ""
            if hops[dep] < 2 and dep.startswith("/Game/LFF/") and cls.endswith(("DataAsset", "ConfigAsset", "Blueprint")):
                queue.append(dep)
    return found


def stadium_world():
    """The editor world when it is L_LFF_Stadium (loading it if nothing unsaved is open), else None."""
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world() is not None:
        warn("PIE is running: stop it to export people/ and read the level's people")
        return None
    world = editor.get_editor_world()
    if world is not None and world.get_path_name().startswith(LEVEL + "."):
        return world
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        warn(f"another level with unsaved changes is open; open {LEVEL} to export people/")
        return None
    log(f"loading {LEVEL} (read only)")
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        return None
    return editor.get_editor_world()


def level_actors(world, cls_name):
    cls = getattr(unreal, cls_name, None)
    if world is None or cls is None:
        return []
    return [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if isinstance(a, cls)]


def people_references(world):
    """Meshes and clips the person actors and the crowd spawner use (PoseMesh is a C++ default, not in the map)."""
    refs = set()
    looks = []
    for actor in level_actors(world, "LFFPersonActor"):
        looks.append(actor)
    for spawner in level_actors(world, "LFFStadiumCrowdSpawner"):
        looks += list(prop(spawner, "detailed_looks") or [])
        refs |= {package_path(prop(spawner, clip)) for clip in SPAWNER_CLIPS}
    for look in looks:
        refs |= {package_path(prop(look, name)) for name in PERSON_PROPS}
        refs.add(package_path(prop(prop(look, "hair_style"), "card_mesh")))
    return {r for r in refs if r}


def discover(world, report):
    """{package path: class name} for every exportable or known-unsupported asset, sorted by path."""
    paths = set(KNOWN_ASSETS) | level_references() | people_references(world)
    for folder in KNOWN_FOLDERS:
        paths |= {p.split(".")[0] for p in unreal.EditorAssetLibrary.list_assets(folder, recursive=True)}
    assets, missing = {}, []
    for path in sorted(paths):
        if path.startswith("/Engine/") or path.startswith("/Script/"):
            continue
        cls = asset_class(path)
        if cls is None:
            if path in KNOWN_ASSETS:
                missing.append(path)
            continue
        if cls in EXPORTABLE or cls in UNSUPPORTED:
            assets[path] = cls
    for path in missing:
        report["skipped"].append({"asset": path, "reason": "not found in this project"})
    return assets


# --- LODs and bounds ---------------------------------------------------------------------------------------------------
def static_lod_counts(mesh):
    """[(lod, triangles or None)]."""
    try:
        n = mesh.get_num_lods()
    except Exception:  # noqa: BLE001
        try:
            n = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_lod_count(mesh)
        except Exception:  # noqa: BLE001
            n = 1
    out = []
    for lod in range(max(n, 1)):
        try:
            out.append((lod, mesh.get_num_triangles(lod)))
        except Exception:  # noqa: BLE001
            out.append((lod, None))
    return out


def skeletal_lod_counts(mesh):
    """[(lod, estimated triangles or None)] from vertex counts."""
    sub = getattr(unreal, "SkeletalMeshEditorSubsystem", None)
    lib = unreal.get_editor_subsystem(sub) if sub else getattr(unreal, "EditorSkeletalMeshLibrary", None)
    try:
        n = lib.get_lod_count(mesh)
    except Exception:  # noqa: BLE001
        return [(0, None)]
    out = []
    for lod in range(max(n, 1)):
        try:
            out.append((lod, int(lib.get_num_verts(mesh, lod) * TRIS_PER_VERT)))
        except Exception:  # noqa: BLE001
            out.append((lod, None))
    return out


def pick_lod(counts, args):
    """(lod, triangles): --lod N (clamped), or the most detailed LOD within --max-triangles (else the lightest)."""
    if args.lod != "auto":
        lod = min(int(args.lod), len(counts) - 1)
        return counts[lod]
    for lod, tris in counts:
        if tris is not None and tris <= args.max_triangles:
            return lod, tris
    known = [c for c in counts if c[1] is not None]
    return min(known, key=lambda c: c[1]) if known else counts[0]


def mesh_bounds(mesh):
    """(min, max) of the asset in its own frame, cm."""
    if isinstance(mesh, unreal.StaticMesh):
        box = mesh.get_bounding_box()
        return vec(box.min), vec(box.max)
    try:
        b = mesh.get_imported_bounds()
    except Exception:  # noqa: BLE001
        b = mesh.get_bounds()
    o, e = b.origin, b.box_extent
    return vec(o - e), vec(o + e)


def nanite_enabled(mesh):
    return bool(prop(prop(mesh, "nanite_settings"), "enabled", False)) if isinstance(mesh, unreal.StaticMesh) else False


def skeleton_of(obj):
    for getter in (lambda: obj.get_editor_property("skeleton"), lambda: obj.skeleton, lambda: obj.get_skeleton()):
        try:
            s = getter()
            if s:
                return s
        except Exception:  # noqa: BLE001
            pass
    return None


def preview_mesh(anim, meshes_by_skeleton):
    """The mesh the exporter will most likely use for a clip (its or its skeleton's preview mesh), for LOD and bounds."""
    skeleton = skeleton_of(anim)
    for owner in (anim, skeleton):
        m = prop(owner, "preview_skeletal_mesh") if owner else None
        if isinstance(m, unreal.SoftObjectPath):
            m = unreal.load_asset(str(m)) if str(m) else None
        if isinstance(m, unreal.SkeletalMesh):
            return m
    path = meshes_by_skeleton.get(package_path(skeleton))
    return unreal.load_asset(path) if path else None


# --- the glTF exporter -------------------------------------------------------------------------------------------------
def enum(type_name, member):
    return getattr(getattr(unreal, type_name, None), member, None)


def set_opt(options, name, value):
    if value is None:
        _option_warnings.add(f"{name}: enum value not found on this engine")
        return
    try:
        options.set_editor_property(name, value)
    except Exception as e:  # noqa: BLE001 - renamed or removed on this engine version
        if name not in {w.split(":")[0] for w in _option_warnings}:
            warn(f"glTF option {name} not set: {e}")
        _option_warnings.add(f"{name}: {e}")


def bake_size(n):
    try:
        return unreal.GLTFMaterialBakeSize(x=n, y=n, auto_detect=False)
    except Exception:  # noqa: BLE001
        size = unreal.GLTFMaterialBakeSize()
        for k, v in (("x", n), ("y", n), ("auto_detect", False)):
            try:
                size.set_editor_property(k, v)
            except Exception:  # noqa: BLE001
                pass
        return size


def gltf_options(args, lod, bake, animation=False):
    o = unreal.GLTFExportOptions()
    mode = {"mesh": "USE_MESH_DATA", "simple": "SIMPLE", "off": "DISABLED"}[bake]
    for name, value in (
        ("export_uniform_scale", GLB_SCALE),
        ("bake_material_inputs", enum("GLTFMaterialBakeMode", mode)),
        ("default_material_bake_size", bake_size(args.texture_size)),
        ("default_level_of_detail", lod),
        ("export_vertex_skin_weights", True),
        ("export_vertex_colors", False),        # UE meshes use vertex colour as masks; glTF would tint with it
        ("use_mesh_quantization", False),
        ("export_animation_sequences", animation),
        ("export_preview_mesh", animation),
        ("export_level_sequences", False),
        ("export_hidden_in_game", False),
        ("export_cameras", False),
        ("export_lights", 0),
        ("texture_image_format", enum("GLTFTextureImageFormat", "JPEG" if args.jpeg else "PNG")),
        ("texture_image_quality", 85),
    ):
        set_opt(o, name, value)
    return o


def run_gltf(obj, filename, options, actors=()):
    """Exports to a temporary .glb outside models/ and moves it into place. Returns (ok, warnings, errors)."""
    if not hasattr(unreal, "GLTFExporter"):
        raise RuntimeError("unreal.GLTFExporter is missing: enable the glTF Exporter plugin (Edit > Plugins) and restart")
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    os.makedirs(PARTIAL, exist_ok=True)
    partial = os.path.join(PARTIAL, os.path.basename(filename))
    try:
        result = unreal.GLTFExporter.export_to_gltf(obj, partial, options, set(actors))
    except TypeError:  # an engine that wants its own TSet wrapper for SelectedActors
        selected = unreal.Set(unreal.Actor)
        for a in actors:
            selected.add(a)
        result = unreal.GLTFExporter.export_to_gltf(obj, partial, options, selected)
    ok, messages = (result[0], result[1]) if isinstance(result, tuple) else (result, None)
    warnings = [str(m) for m in (prop(messages, "warnings") or [])] if messages else []
    errors = [str(m) for m in (prop(messages, "errors") or [])] if messages else []
    if ok and os.path.isfile(partial) and os.path.getsize(partial) > 0:
        os.replace(partial, filename)
        return True, warnings, errors
    if os.path.isfile(partial):
        os.remove(partial)
    return False, warnings, errors or ["the exporter wrote nothing"]


# --- GLB summary (for the report) --------------------------------------------------------------------------------------
def image_size(blob):
    if blob[:8] == b"\x89PNG\r\n\x1a\n":
        return list(struct.unpack(">II", blob[16:24]))
    i = 2
    while blob[:2] == b"\xff\xd8" and i + 9 < len(blob):
        marker, length = blob[i + 1], struct.unpack(">H", blob[i + 2:i + 4])[0]
        if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
            h, w = struct.unpack(">HH", blob[i + 5:i + 9])
            return [w, h]
        i += 2 + length
    return None


def glb_summary(path, cap):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"glTF":
        return {}
    doc, binary, off = {}, b"", 12
    while off + 8 <= len(data):
        length, kind = struct.unpack_from("<II", data, off)
        chunk = data[off + 8:off + 8 + length]
        off += 8 + length
        if kind == 0x4E4F534A:
            doc = json.loads(chunk)
        elif kind == 0x004E4942:
            binary = chunk
    accessors = doc.get("accessors", [])
    tris = 0
    for mesh in doc.get("meshes", []):
        for p in mesh.get("primitives", []):
            if p.get("mode", 4) == 4:
                index = p.get("indices", p.get("attributes", {}).get("POSITION"))
                tris += accessors[index]["count"] // 3 if index is not None else 0
    images = []
    for img in doc.get("images", []):
        view = doc["bufferViews"][img["bufferView"]] if "bufferView" in img else None
        blob = binary[view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]] if view else b""
        images.append({"name": img.get("name"), "mime": img.get("mimeType"), "size": image_size(blob), "bytes": len(blob)})
    return {"triangles": tris, "meshes": len(doc.get("meshes", [])), "materials": len(doc.get("materials", [])),
            "skins": len(doc.get("skins", [])), "animations": len(doc.get("animations", [])), "images": images,
            "oversizeImages": [i for i in images if i["size"] and max(i["size"]) > cap]}


# --- exports -----------------------------------------------------------------------------------------------------------
class Run:
    """Shared state: arguments, report, up-to-date records, progress."""

    def __init__(self, args, bake):
        self.args, self.bake = args, bake
        self.report = {"exported": [], "skipped": [], "removed": [], "people": [], "looks": [], "ticket": {}}
        self.state = {}
        if os.path.isfile(STATE):
            with open(STATE) as f:
                self.state = json.load(f)
        self.names = {}

    def signature(self, **extra):
        a = self.args
        key = {"v": SCRIPT_VERSION, "bake": self.bake, "texture": a.texture_size, "jpeg": a.jpeg, "scale": GLB_SCALE, **extra}
        return hashlib.sha1(json.dumps(key, sort_keys=True).encode()).hexdigest()[:16]

    def fresh(self, out, source, mtime, sig, group):
        if self.args.force or not os.path.isfile(out):
            return False
        return self.state.get(rel(out)) == {"source": source, "mtime": mtime, "sig": sig, "group": group}

    def record(self, out, source, mtime, sig, group):
        self.state[rel(out)] = {"source": source, "mtime": mtime, "sig": sig, "group": group}

    def skip(self, asset, reason):
        self.report["skipped"].append({"asset": asset, "reason": reason})
        log(f"skip {asset}: {reason}")

    def claim(self, name, path):
        """Outputs are named by asset name only: refuse a second asset with the same name."""
        if self.names.setdefault(name, path) != path:
            self.skip(path, f"name collision with {self.names[name]} (outputs are named by asset name only)")
            return False
        return True


def export_mesh(run, path, cls, step):
    mesh = unreal.load_asset(path)
    name = mesh.get_name()
    if not run.claim(name, path):
        return
    counts = static_lod_counts(mesh) if cls == "StaticMesh" else skeletal_lod_counts(mesh)
    lod, tris = pick_lod(counts, run.args)
    out = os.path.join(MODELS, f"{name}.glb")
    lo, hi = mesh_bounds(mesh)
    entry = {"asset": path, "group": group_of(path), "kind": EXPORTABLE[cls], "file": rel(out), "lod": lod,
             "lodCount": len(counts), "estimatedTriangles" if cls != "StaticMesh" else "sourceTriangles": tris,
             "nanite": nanite_enabled(mesh)}
    if export(run, mesh, path, out, run.signature(lod=lod), gltf_options(run.args, lod, run.bake), entry, step):
        write_json(os.path.join(MODELS, f"{name}.bounds.json"),
                   {"asset": path, "min": lo, "max": hi, "units": "cm", "frame": "unreal-local"})


def export_anim(run, path, step, meshes_by_skeleton):
    anim = unreal.load_asset(path)
    name = anim.get_name()
    if not run.claim(name, path):
        return
    out = os.path.join(MODELS, "anim", f"{name}.glb")
    mesh = preview_mesh(anim, meshes_by_skeleton)
    lod, tris = pick_lod(skeletal_lod_counts(mesh), run.args) if mesh else (0, None)
    entry = {"asset": path, "group": group_of(path), "kind": "animation", "file": rel(out), "lod": lod,
             "previewMesh": package_path(mesh), "skeleton": package_path(skeleton_of(anim)), "estimatedTriangles": tris}
    try:
        entry["seconds"] = round(anim.get_play_length(), 3)
    except Exception:  # noqa: BLE001
        pass
    # Clips need the skeleton, not the look: no material baking.
    if export(run, anim, path, out, run.signature(lod=lod, anim=True),
              gltf_options(run.args, lod, "off", animation=True), entry, step) and mesh:
        lo, hi = mesh_bounds(mesh)
        write_json(os.path.join(MODELS, "anim", f"{name}.bounds.json"),
                   {"asset": path, "previewMesh": package_path(mesh), "min": lo, "max": hi, "units": "cm",
                    "frame": "unreal-local"})


def export(run, obj, path, out, sig, options, entry, step, actors=(), mtime=None):
    """Exports one GLB unless it is up to date. True when the GLB is there (exported or up to date)."""
    mtime = source_mtime(obj) if mtime is None else mtime
    tag = f"{step} {os.path.basename(out)}"
    if run.args.dry_run:
        entry["status"] = "dry-run"
        run.report["exported"].append(entry)
        log(f"{tag}: would export (LOD {entry.get('lod')})")
        return False
    if run.fresh(out, path, mtime, sig, entry["group"]):
        entry.update(status="up-to-date", bytes=os.path.getsize(out))
        run.report["exported"].append(entry)
        log(f"{tag}: up to date")
        return True
    t = time.time()
    ok, warnings, errors = run_gltf(obj, out, options, actors)
    for w in warnings:
        warn(f"{tag}: {w}")
    if not ok:
        run.skip(path, "export failed: " + "; ".join(errors))
        return False
    run.record(out, path, mtime, sig, entry["group"])
    entry.update(status="exported", bytes=os.path.getsize(out), seconds=round(time.time() - t, 1), warnings=warnings,
                 glb=glb_summary(out, run.args.texture_size))
    run.report["exported"].append(entry)
    big = len(entry["glb"].get("oversizeImages", []))
    log(f"{tag}: LOD {entry.get('lod')}, {entry['glb'].get('triangles', 0):,} triangles, "
        f"{entry['bytes'] / 1024:,.0f} KB{f', {big} images over {run.args.texture_size} px' if big else ''} "
        f"({entry['seconds']} s)")
    return True


def tee_colour(material):
    try:
        c = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, TEE_COLOUR_PARAM)
        return [round(c.r, 4), round(c.g, 4), round(c.b, 4)]
    except Exception:  # noqa: BLE001
        return None


def describe_look(look):
    """Body, face, hair and outfit of an ALFFPersonActor or an FLFFCrowdAppearance (same property names)."""
    def glb(obj):
        return f"{obj.get_name()}.glb" if obj else None
    body, face = prop(look, "body_mesh"), prop(look, "face_mesh")
    hair = prop(prop(look, "hair_style"), "card_mesh")
    outfit = [{"cloth": package_path(prop(p, "cloth")), "material": package_path(prop(p, "material")),
               "colour": tee_colour(prop(p, "material")) if prop(p, "material") else None}
              for p in (prop(look, "outfit") or [])]
    return {"body": package_path(body), "bodyGlb": glb(body), "face": package_path(face), "faceGlb": glb(face),
            "hair": package_path(hair), "hairGlb": glb(hair), "outfit": outfit}


def export_people(run, world, step):
    for i, spawner in enumerate(level_actors(world, "LFFStadiumCrowdSpawner")):
        for n, look in enumerate(prop(spawner, "detailed_looks") or []):
            run.report["looks"].append({"name": f"Look{n}", "spawner": spawner.get_actor_label(), **describe_look(look)})
    if "people" not in run.args.only:
        return
    actors = level_actors(world, "LFFPersonActor")
    if not actors:
        run.skip(LEVEL, "people/: no LFFPersonActor in the open level (or the class isn't loaded)")
        return
    level_mtime = round(os.path.getmtime(LEVEL_FILE), 3) if os.path.isfile(LEVEL_FILE) else None
    for k, actor in enumerate(actors):
        label = actor.get_actor_label()
        out = os.path.join(MODELS, "people", f"{label}.glb")
        body = prop(actor, "body_mesh")
        lod, tris = pick_lod(skeletal_lod_counts(body), run.args) if body else (0, None)
        t = actor.get_actor_transform()
        r = t.rotation.rotator()
        transform = {"location": vec(t.translation), "rotation": {"pitch": round(r.pitch, 4), "yaw": round(r.yaw, 4),
                     "roll": round(r.roll, 4)}, "scale": vec(t.scale3d)}
        look = describe_look(actor)
        entry = {"asset": f"{LEVEL}:{label}", "group": "metahuman", "kind": "person", "file": rel(out), "lod": lod,
                 "estimatedTriangles": tris, "actorTransform": transform, **look,
                 "clips": {n: package_path(prop(actor, n)) for n in ("still_animation", "moving_animation", "facial_animation")},
                 "note": "no outfit: Chaos cloth isn't exportable"}
        run.report["people"].append(entry)
        if export(run, world, f"{LEVEL}:{label}", out, run.signature(lod=lod, person=True),
                  gltf_options(run.args, lod, run.bake), dict(entry), f"{step} {k + 1}/{len(actors)}",
                  actors=[actor], mtime=level_mtime):
            origin, extent = actor.get_actor_bounds(False)
            write_json(os.path.join(MODELS, "people", f"{label}.bounds.json"),
                       {"asset": f"{LEVEL}:{label}", "min": vec(origin - extent), "max": vec(origin + extent),
                        "units": "cm", "frame": "unreal-world", "actorTransform": transform})


def ticket_textures():
    found = []
    for path in unreal.EditorAssetLibrary.list_assets(TICKET_DIR, recursive=False):
        m = re.search(r"T_LFF_Ticket_(\d+)$", path.split(".")[0])
        if m:
            found.append((int(m.group(1)), path.split(".")[0]))
    return sorted(found)


def export_ticket(run):
    frames = ticket_textures()
    info = {"frames": [], "expected": TICKET_FRAMES, "found": len(frames)}
    run.report["ticket"] = info
    if len(frames) != TICKET_FRAMES:
        warn(f"ticket: {len(frames)} frames in {TICKET_DIR}, expected {TICKET_FRAMES}")
    for index, (number, path) in enumerate(frames):
        out = os.path.join(TICKET_OUT, f"{index:02d}.png")
        texture = unreal.load_asset(path)
        mtime, sig = source_mtime(texture), run.signature(ticket=True)
        if run.args.dry_run:
            info["frames"].append({"asset": path, "file": rel(out), "status": "dry-run"})
            continue
        if run.fresh(out, path, mtime, sig, "lff"):
            info["frames"].append({"asset": path, "file": rel(out), "status": "up-to-date", "bytes": os.path.getsize(out)})
            continue
        how = export_texture_png(texture, out) or copy_ticket_source(number, out)
        if not how:
            run.skip(path, "ticket frame export failed and Saved/TicketImport has no source PNG")
            continue
        run.record(out, path, mtime, sig, "lff")
        with open(out, "rb") as f:
            size = image_size(f.read(32))
        info["frames"].append({"asset": path, "file": rel(out), "status": "exported", "via": how, "size": size,
                               "bytes": os.path.getsize(out)})
    sizes = {tuple(f["size"]) for f in info["frames"] if f.get("size")}
    log(f"ticket: {len(info['frames'])} frames to {rel(TICKET_OUT)}/" + (f", {sorted(sizes)} px" if sizes else ""))


def export_texture_png(texture, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    task = unreal.AssetExportTask()
    for name, value in (("object", texture), ("filename", out), ("automated", True), ("replace_identical", True),
                        ("prompt", False)):
        task.set_editor_property(name, value)
    exporter = getattr(unreal, "TextureExporterPNG", None)
    if exporter:
        task.set_editor_property("exporter", exporter())
    try:
        ok = unreal.Exporter.run_asset_export_task(task)
    except Exception as e:  # noqa: BLE001
        warn(f"ticket {texture.get_name()}: {e}")
        ok = False
    return "unreal" if ok and os.path.isfile(out) else None


def copy_ticket_source(number, out):
    src = os.path.join(TICKET_FALLBACK, f"T_LFF_Ticket_{number:02d}.png")
    if not os.path.isfile(src):
        return None
    shutil.copyfile(src, out)
    return "Saved/TicketImport"


def prune_disabled_groups(run):
    """Deletes this script's own earlier outputs for licence groups that are now off."""
    for key, rec in list(run.state.items()):
        if rec.get("group") in run.args.groups:
            continue
        for path in (os.path.join(OUT, key), os.path.join(OUT, key[:-4] + ".bounds.json")):
            if os.path.isfile(path):
                if run.args.dry_run:
                    run.report["removed"].append({"file": rel(path), "status": "would remove"})
                    continue
                os.remove(path)
                run.report["removed"].append({"file": rel(path), "group": rec.get("group"),
                                              "reason": f"{FLAGS.get(rec.get('group'), 'group')} not given"})
                log(f"removed {rel(path)} ({rec.get('group')} is off)")
        if not run.args.dry_run:
            del run.state[key]


# --- main --------------------------------------------------------------------------------------------------------------
def licence_notices(args):
    notices = {}
    for group, flag in FLAGS.items():
        on = group in args.groups
        notices[group] = {"included": on, "flag": flag, "notice": LICENCE[group]}
        if on:
            unreal.log_warning(f"[{TAG}] LICENCE: {LICENCE[group]}")
    return notices


def main():
    args = parse_args()
    if not hasattr(unreal, "GLTFExporter") and not args.dry_run and set(args.only) - {"ticket"}:
        raise RuntimeError("unreal.GLTFExporter is missing: enable the glTF Exporter plugin (Edit > Plugins) and restart")
    bake = args.bake
    if nullrhi() and bake != "off":
        warn("-nullrhi: material baking needs a GPU, so it is off for this run (run in the open editor to bake)")
        bake = "off"
    run = Run(args, bake)
    run.report.update(generatedAt=time.strftime("%Y-%m-%dT%H:%M:%S%z"), script="Tools/Editor/export_web_assets.py",
                      untested="written without an editor; check the log and optionWarnings",
                      engine=unreal.SystemLibrary.get_engine_version(), dryRun=args.dry_run,
                      settings={"groups": sorted(args.groups), "only": args.only, "force": args.force,
                                "textureSize": args.texture_size, "lod": args.lod, "maxTriangles": args.max_triangles,
                                "bake": bake, "bakeRequested": args.bake, "jpeg": args.jpeg,
                                "glbUnits": f"m (export_uniform_scale {GLB_SCALE})", "boundsUnits": "cm"},
                      licence=licence_notices(args))
    prune_disabled_groups(run)

    world = stadium_world() if "metahuman" in args.groups else None
    assets = discover(world, run.report) if set(args.only) & {"models", "anims"} else {}
    meshes_by_skeleton = {}
    for path, cls in assets.items():
        if cls == "SkeletalMesh" and group_of(path) in args.groups:
            meshes_by_skeleton.setdefault(package_path(skeleton_of(unreal.load_asset(path))), path)
    if assets.get(POSE_MESH) == "SkeletalMesh":  # the crowd clips' skeleton: prefer the mesh the people play them on
        meshes_by_skeleton[package_path(skeleton_of(unreal.load_asset(POSE_MESH)))] = POSE_MESH
    meshes_by_skeleton.pop(None, None)

    todo = list(assets.items())
    for n, (path, cls) in enumerate(todo):
        step = f"{n + 1}/{len(todo)}"
        group = group_of(path) or "lff"
        if group not in args.groups:
            run.skip(path, f"{FLAGS[group]} not given (licence: see export_report.json 'licence')")
        elif cls in UNSUPPORTED:
            run.skip(path, UNSUPPORTED[cls])
        elif cls == "AnimSequence":
            if "anims" in args.only:
                export_anim(run, path, step, meshes_by_skeleton)
        elif "models" in args.only:
            export_mesh(run, path, cls, step)

    if "metahuman" in args.groups and set(args.only) & {"people", "models"}:
        if world is None:
            run.skip(LEVEL, "people/ and looks: L_LFF_Stadium isn't the open level (see the warning above)")
        else:
            export_people(run, world, "people")
    if "ticket" in args.only:
        export_ticket(run)

    done = [e for e in run.report["exported"] if e.get("status") == "exported"]
    run.report["optionWarnings"] = sorted(_option_warnings)
    run.report["totals"] = {"exported": len(done), "upToDate": sum(e.get("status") == "up-to-date" for e in run.report["exported"]),
                            "skipped": len(run.report["skipped"]), "removed": len(run.report["removed"]),
                            "bytes": sum(e.get("bytes", 0) for e in run.report["exported"] + run.report["ticket"].get("frames", []))}
    os.makedirs(OUT, exist_ok=True)
    write_json(REPORT, run.report)
    if not args.dry_run:
        write_json(STATE, run.state)
    t = run.report["totals"]
    log(f"DONE: {t['exported']} exported, {t['upToDate']} up to date, {t['skipped']} skipped, {t['removed']} removed, "
        f"{t['bytes'] / 1048576:,.1f} MB; report {rel(REPORT)}")


if __name__ == "__main__":
    _quit = "--quit" in os.environ.get("LFF_WEB_EXPORT_ARGS", "") or "--quit" in getattr(sys, "argv", [])
    try:
        main()
    except (Exception, SystemExit) as e:  # noqa: BLE001 - report and let run_headless.sh stop the editor
        import traceback
        unreal.log_error(f"[{TAG}] FAILED: {e}\n{traceback.format_exc()}")
    finally:
        if _quit:
            unreal.SystemLibrary.quit_editor()
