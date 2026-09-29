"""Imports the prepared CGTrader stadium into /Game/LFF/Stadium and swaps it into L_LFF_Stadium.

Run headless with the editor closed, after Tools/Pipeline/prepare_stadium.py (Docs/ASSET_INGESTION.md):
  UnrealEditor VirtualStadium.uproject -ExecutePythonScript="<abs path to this file>" -ModelContextProtocolPort=18000 \
      -unattended -nullrhi -nopause -nosplash -stdout

Inputs (never committed): Saved/StadiumImport/v1/*.fbx + manifest.json, and the source textures and MTL files in
Assets/Eagles Stadium Philadelphia 3D Model/OBJ File format (Exchange).

What it does:
  1. Textures -> /Game/LFF/Stadium/Textures (normal maps as TC_Normalmap/linear; streaming on; World LOD group).
  2. Master materials M_LFF_Stadium (opaque) and M_LFF_StadiumMasked (alpha cut-outs), both two-sided, and one
     instance per source material with physically plausible roughness/metallic by name (the source only has
     base color and normal maps).
  3. Chunks -> /Game/LFF/Stadium/Meshes as Nanite static meshes, source normals kept, no collision (camera-only
     tour), 32 Lumen cards and two-sided distance fields for the thin roof and canopy geometry.
  4. Level: LFF_Model under LFF_SiteRoot with one actor per chunk (geometry is already in the site frame);
     a CesiumCartographicPolygon over the model site plus a CesiumPolygonRasterOverlay on Tileset_Context that
     clips the Google tiles there; the proxy concourse is removed.
Idempotent for assets (skips existing ones); refuses to touch the level if LFF_Model already exists.
"""
import json
import os
import re

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PREP_DIR = os.path.join(PROJECT_DIR, "Saved", "StadiumImport", "v1")
SRC_DIR = os.path.join(PROJECT_DIR, "Assets", "Eagles Stadium Philadelphia 3D Model", "OBJ File format (Exchange)")
ROOT = "/Game/LFF/Stadium"
TEX_DIR, MAT_DIR, MESH_DIR = f"{ROOT}/Textures", f"{ROOT}/Materials", f"{ROOT}/Meshes"
LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
MTL_FILES = ["Eagles Stadium Philadelphia 3D Model.mtl", "Football Field NFL.mtl"] + \
            [f"3D seats/3D Seats Part{i}.mtl" for i in range(1, 6)]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def log(msg):
    unreal.log(f"[LFF stadium] {msg}")


def asset_name(s):
    return re.sub(r"[^A-Za-z0-9_]", "_", os.path.splitext(s)[0]).strip("_")


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


# ---------------------------------------------------------------- MTL parsing
def parse_mtls():
    mats = {}
    for rel in MTL_FILES:
        base = os.path.dirname(os.path.join(SRC_DIR, rel))
        cur = None
        for line in open(os.path.join(SRC_DIR, rel), encoding="utf-8", errors="ignore"):
            parts = line.strip().split(None, 1)
            if not parts:
                continue
            key, val = parts[0], (parts[1] if len(parts) > 1 else "")
            if key == "newmtl":
                cur = mats.setdefault(val.strip(), {"name": val.strip()})
            elif cur is None:
                continue
            elif key == "Kd":
                cur["kd"] = [float(x) for x in val.split()[:3]]
            elif key == "map_Kd":
                cur["base"] = os.path.normpath(os.path.join(base, val.split()[-1] if "-" in val.split()[0] else val.strip()))
            elif key in ("map_Bump", "map_bump", "bump", "norm"):
                cur["normal"] = os.path.normpath(os.path.join(base, val.split()[-1]))
    return mats


# ---------------------------------------------------------------- 1. textures
def import_textures(paths):
    textures = {}
    todo = []
    for p in sorted(paths):
        name = "T_" + asset_name(os.path.basename(p))
        dest = f"{TEX_DIR}/{name}"
        if eal.does_asset_exist(dest):
            textures[p] = unreal.load_asset(dest)
            continue
        task = unreal.AssetImportTask()
        for k, v in (("filename", p), ("destination_path", TEX_DIR), ("destination_name", name), ("automated", True),
                     ("save", False), ("replace_existing", True)):
            task.set_editor_property(k, v)
        todo.append((p, dest, task))
    if todo:
        tools.import_asset_tasks([t for _, _, t in todo])
    for p, dest, _ in todo:
        tex = unreal.load_asset(dest)
        if not tex:
            unreal.log_warning(f"[LFF stadium] texture failed: {p}")
            continue
        is_normal = "normal" in os.path.basename(p).lower()
        if is_normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if is_normal else unreal.TextureGroup.TEXTUREGROUP_WORLD)
        eal.save_loaded_asset(tex)
        textures[p] = tex
    log(f"textures: {len(textures)} ({len(todo)} imported)")
    return textures


# ---------------------------------------------------------------- 2. materials
def make_master(name, masked):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    if masked:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("opacity_mask_clip_value", 0.4)
    E = unreal.MaterialExpressionTextureSampleParameter2D
    base = mel.create_material_expression(m, E, -900, -200)
    base.set_editor_property("parameter_name", "BaseColorMap")
    base.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 50)
    tint.set_editor_property("parameter_name", "BaseTint"); tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, -150)
    mel.connect_material_expressions(base, "RGB", mul, "A"); mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nrm = mel.create_material_expression(m, E, -900, 300)
    nrm.set_editor_property("parameter_name", "NormalMap")
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    nrm.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    for i, (pname, default, prop) in enumerate((("Roughness", 0.7, unreal.MaterialProperty.MP_ROUGHNESS),
                                                ("Metallic", 0.0, unreal.MaterialProperty.MP_METALLIC),
                                                ("Specular", 0.5, unreal.MaterialProperty.MP_SPECULAR))):
        s = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 100 + i * 120)
        s.set_editor_property("parameter_name", pname); s.set_editor_property("default_value", default)
        mel.connect_material_property(s, "", prop)
    em = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 500)
    em.set_editor_property("parameter_name", "EmissiveStrength"); em.set_editor_property("default_value", 0.0)
    emul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 450)
    mel.connect_material_expressions(mul, "", emul, "A"); mel.connect_material_expressions(em, "", emul, "B")
    mel.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if masked:
        mel.connect_material_property(base, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


# name pattern -> (roughness, metallic, specular, emissive). First match wins. The source has no PBR maps.
PBR_RULES = [
    (r"glass", (0.06, 0.2, 0.9, 0.0)),        # opaque glossy glass: keeps Nanite; reads as glazing at stadium scale
    (r"solar", (0.25, 0.4, 0.6, 0.0)),
    (r"^led|display|lednr|scoreboard", (0.4, 0.0, 0.5, 3.0)),  # static glow only; nothing animates or flashes
    (r"metal|grill|gate|soundbox", (0.42, 0.75, 0.5, 0.0)),
    (r"chair", (0.45, 0.0, 0.5, 0.0)),        # moulded plastic seats
    (r"grass", (0.92, 0.0, 0.35, 0.0)),
    (r"paint|^white$|^black$|^yellow$|^blue$|^orange$", (0.5, 0.0, 0.5, 0.0)),
    (r"banner|flag|curtain|cover|mesh|custom", (0.65, 0.0, 0.45, 0.0)),
    (r"concrete|wall|brick|tile|floor|asphalt|loading|door|exit|adb", (0.85, 0.0, 0.45, 0.0)),
]


# Base colour tints for textured materials whose albedo is unrealistically bright under a physical sun.
# The turf texture averages sRGB (62,121,26); real turf reflects ~8-12%. Only the green turf ("Grass"), not the
# white/yellow painted-line materials ("W grass", "Y grass").
TINT_RULES = {"grass": (0.55, 0.6, 0.5)}
SPECULAR_OVERRIDES = {"grass": 0.25}


def pbr_for(name):
    n = name.lower()
    for pat, vals in PBR_RULES:
        if re.search(pat, n):
            return vals
    return (0.7, 0.0, 0.5, 0.0)


def has_alpha(tex):
    try:
        return tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_NORMALMAP and \
            tex.get_editor_property("source").get_format() in (unreal.TextureSourceFormat.TSF_BGRA8, unreal.TextureSourceFormat.TSF_RGBA16) and \
            tex.get_editor_property("compression_no_alpha") is False and tex.has_alpha_channel()
    except Exception:
        return False


def make_instances(mtls, textures, opaque, masked):
    instances = {}
    for name, info in sorted(mtls.items()):
        mi_name = "MI_LFF_" + asset_name(name)
        path = f"{MAT_DIR}/{mi_name}"
        if eal.does_asset_exist(path):
            instances[norm(name)] = unreal.load_asset(path)
            continue
        base = textures.get(info.get("base"))
        cutout = base is not None and info.get("base", "").lower().endswith(".png") and has_alpha(base)
        mi = tools.create_asset(mi_name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", masked if cutout else opaque)
        if base:
            mel.set_material_instance_texture_parameter_value(mi, "BaseColorMap", base)
        nt = textures.get(info.get("normal"))
        if nt:
            mel.set_material_instance_texture_parameter_value(mi, "NormalMap", nt)
        kd = info.get("kd")
        if kd and not base:
            mel.set_material_instance_vector_parameter_value(mi, "BaseTint", unreal.LinearColor(*kd, 1.0))
        if norm(name) in TINT_RULES:
            mel.set_material_instance_vector_parameter_value(mi, "BaseTint", unreal.LinearColor(*TINT_RULES[norm(name)], 1.0))
        r, m, s, e = pbr_for(name)
        s = SPECULAR_OVERRIDES.get(norm(name), s)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", r)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", m)
        mel.set_material_instance_scalar_parameter_value(mi, "Specular", s)
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", e)
        eal.save_loaded_asset(mi)
        instances[norm(name)] = mi
    log(f"material instances: {len(instances)}")
    return instances


# ---------------------------------------------------------------- 3. meshes
def import_pipeline():
    path = f"{ROOT}/Import/IP_LFF_StadiumMeshes"
    if not eal.does_asset_exist(path):
        eal.duplicate_asset("/Interchange/Pipelines/DefaultFBXOBJAssetsPipeline", path)
    p = unreal.load_asset(path)
    mp, cp, matp = p.get_editor_property("mesh_pipeline"), p.get_editor_property("common_meshes_properties"), p.get_editor_property("material_pipeline")
    for obj, k, v in ((mp, "build_nanite", True), (mp, "collision", False), (mp, "generate_lightmap_u_vs", False),
                      (mp, "max_lumen_mesh_cards", 32), (mp, "generate_distance_field_as_if_two_sided", True),
                      (mp, "import_skeletal_meshes", False), (cp, "recompute_normals", False),
                      (cp, "import_lods", False), (matp, "import_materials", False)):
        try:
            obj.set_editor_property(k, v)
        except Exception as e:
            unreal.log_warning(f"[LFF stadium] pipeline {k}: {e}")
    eal.save_loaded_asset(p)
    return p


def import_meshes(manifest, instances):
    pipeline = import_pipeline()
    meshes = {}
    for name, info in manifest["chunks"].items():
        dest = f"{MESH_DIR}/{name}"
        if not eal.does_asset_exist(dest):
            src = unreal.InterchangeManager.create_source_data(os.path.join(PREP_DIR, info["file"]))
            params = unreal.ImportAssetParameters()
            params.is_automated = True
            params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
            unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(MESH_DIR, src, params)
        mesh = unreal.load_asset(dest)
        if not mesh:
            raise RuntimeError(f"mesh import failed: {name}")
        missing = []
        for i, sm in enumerate(mesh.static_materials):
            slot = str(sm.material_slot_name) or str(sm.imported_material_slot_name)
            mi = instances.get(norm(slot))
            if mi is None:  # Interchange may add numeric suffixes to slot names
                mi = instances.get(norm(re.sub(r"_?\d+$", "", slot)))
            if mi:
                mesh.set_material(i, mi)
            else:
                missing.append(slot)
        ns = mesh.get_editor_property("nanite_settings")
        if not ns.enabled:
            ns.enabled = True
            mesh.set_editor_property("nanite_settings", ns)
        eal.save_loaded_asset(mesh)
        meshes[name] = mesh
        log(f"mesh {name}: {len(mesh.static_materials)} slots, nanite={mesh.get_editor_property('nanite_settings').enabled}"
            + (f", unmatched slots {missing}" if missing else ""))
    return meshes


# ---------------------------------------------------------------- 4. level
def find_actor(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def attach(child, parent):
    child.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE,
                          unreal.AttachmentRule.KEEP_RELATIVE, False)


def place_in_level(manifest, meshes):
    if not les.load_level(LEVEL):
        raise RuntimeError("load_level failed")
    if find_actor("LFF_Model"):
        log("LFF_Model already in the level; not touching it")
        return
    root = find_actor("LFF_SiteRoot")
    model = eas.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0))
    model.set_actor_label("LFF_Model")
    handles = sds.k2_gather_subobject_data_for_instance(model)
    sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.SceneComponent, blueprint_context=None))
    attach(model, root)
    model.set_folder_path("LFF_SiteRoot/LFF_Model")
    for name, mesh in meshes.items():
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
        a.set_actor_label(name)
        smc = a.static_mesh_component
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)  # children of the anchored root must be movable
        smc.set_static_mesh(mesh)
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        attach(a, model)
        a.set_folder_path("LFF_SiteRoot/LFF_Model")

    # The model's geometry is in the site frame: +Y points to the north end in Blender, which Unreal's FBX import
    # flips to the site's +Y = south. Relative identity under LFF_SiteRoot is therefore the calibrated placement.

    # Clip the Google tiles over the model site (the model has its own plaza and lots).
    poly = eas.spawn_actor_from_class(unreal.CesiumCartographicPolygon, unreal.Vector(0, 0, 0))
    poly.set_actor_label("CartoPoly_LFF_Footprint")
    poly.set_folder_path("Cesium")
    spline = poly.get_editor_property("polygon")
    spline.clear_spline_points(False)
    xform = root.get_actor_transform()
    for x_m, y_m in manifest["clip_polygon_xy_m"]:
        local = unreal.Vector(x_m * 100.0, -y_m * 100.0, 500.0)
        spline.add_spline_point(xform.transform_location(local), unreal.SplineCoordinateSpace.WORLD, False)
    for i in range(spline.get_number_of_spline_points()):
        spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
    spline.set_closed_loop(True, True)
    tiles = find_actor("Tileset_Context")
    handles = sds.k2_gather_subobject_data_for_instance(tiles)
    sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.CesiumPolygonRasterOverlay, blueprint_context=None))
    overlay = tiles.get_components_by_class(unreal.CesiumPolygonRasterOverlay)[-1]
    overlay.set_editor_property("polygons", [poly])
    overlay.set_editor_property("exclude_selected_tiles", True)
    overlay.set_editor_property("invert_selection", False)

    # The real model replaces the proxy concourse (whose box also collided with the old tile stadium).
    removed = [a.get_actor_label() for a in eas.get_all_level_actors() if a.get_actor_label().startswith("PX_Concourse")]
    for a in [a for a in eas.get_all_level_actors() if a.get_actor_label().startswith("PX_Concourse")]:
        eas.destroy_actor(a)
    log(f"placed {len(meshes)} chunks under LFF_Model; clip polygon {len(manifest['clip_polygon_xy_m'])} pts; removed {removed}")
    if not les.save_current_level():
        raise RuntimeError("save_current_level failed")


try:
    manifest = json.load(open(os.path.join(PREP_DIR, "manifest.json")))
    mtls = parse_mtls()
    used = set()
    for c in manifest["chunks"].values():
        used |= {norm(m) for m in c["materials"]}
    mtls = {k: v for k, v in mtls.items() if norm(k) in used}
    tex_paths = {v[k] for v in mtls.values() for k in ("base", "normal") if k in v and os.path.exists(v[k])}
    textures = import_textures(tex_paths)
    opaque, masked = make_master("M_LFF_Stadium", False), make_master("M_LFF_StadiumMasked", True)
    instances = make_instances(mtls, textures, opaque, masked)
    meshes = import_meshes(manifest, instances)
    place_in_level(manifest, meshes)
    log("DONE")
except Exception as e:
    import traceback
    unreal.log_error(f"[LFF stadium] FAILED: {e}\n{traceback.format_exc()}")
finally:
    unreal.SystemLibrary.quit_editor()
