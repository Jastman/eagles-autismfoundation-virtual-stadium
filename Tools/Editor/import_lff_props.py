"""Imports the prop models from Assets/ (git-ignored, like the stadium) into /Game/LFF/Props, one static mesh each.

  Assets/bag.gltf               ->  /Game/LFF/Props/Bag/SM_LFF_Bag                (clear bag, step 06)
  Assets/barrier.gltf           ->  /Game/LFF/Props/Barrier/SM_LFF_Barrier        (queue barriers, step 04)
  Assets/metal_detector.gltf    ->  /Game/LFF/Props/MetalDetector/SM_LFF_MetalDetector  (security, step 05)
  Assets/concession-stand.gltf  ->  /Game/LFF/Props/Concession/SM_LFF_Concession  (food, step 22)
  Assets/sensoryVehicle.gltf    ->  /Game/LFF/Props/SensoryVehicle/SM_LFF_SensoryVehicle  (step 11; its side wrap
                                    from Tools/Pipeline/prepare_sav_wrap.py when that has run)

Interchange glTF import with the parts combined into one mesh, no collision (camera-only tour), Nanite off (the props
are small; the clear bag and the stand's glass are translucent). Safe to rerun: it replaces the assets in place.
Run inside the open editor (Unreal MCP execute_python_code):
  import runpy; runpy.run_path(r"<repo>/Tools/Editor/import_lff_props.py", run_name="__main__")
or headless: Tools/Editor/run_headless.sh Tools/Editor/import_lff_props.py "LFF props"
"""
import base64
import json
import os
import runpy

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ROOT = "/Game/LFF/Props"
PROPS = {"bag": ("Bag", "SM_LFF_Bag"), "barrier": ("Barrier", "SM_LFF_Barrier"),
         "metal_detector": ("MetalDetector", "SM_LFF_MetalDetector"),
         "concession-stand": ("Concession", "SM_LFF_Concession"),
         "sensoryVehicle": ("SensoryVehicle", "SM_LFF_SensoryVehicle")}


def pipeline():
    p = unreal.InterchangeGenericAssetsPipeline()
    mesh = p.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mesh.set_editor_property("collision", False)
    mesh.set_editor_property("build_nanite", False)
    common = p.get_editor_property("common_meshes_properties")
    common.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    return p


def unique_materials(source):
    """The files reuse material names ("Mat" on the bag's clear plastic and on its phone, "frame" twice on the metal
    detector), and the combined import merges materials by name. Writes a copy with each name made unique
    (Mat, Mat_2, ...) to Saved/PropImport and returns its path."""
    with open(os.path.join(PROJECT_DIR, "Assets", f"{source}.gltf")) as f:
        doc = json.load(f)
    seen = {}
    for m in doc.get("materials", []):
        base = m.get("name") or "Material"
        seen[base] = seen.get(base, 0) + 1
        if seen[base] > 1:
            m["name"] = f"{base}_{seen[base]}"
    out_dir = os.path.join(PROJECT_DIR, "Saved", "PropImport")
    wrap = os.path.join(out_dir, "T_LFF_SAV_Wrap.jpg")  # Tools/Pipeline/prepare_sav_wrap.py: the real trailer's side
    if source == "sensoryVehicle" and os.path.isfile(wrap):
        with open(wrap, "rb") as f:
            doc["images"][1]["uri"] = "data:image/jpeg;base64," + base64.b64encode(f.read()).decode()
        unreal.log("[LFF props] sensoryVehicle: side wrap from Saved/PropImport/T_LFF_SAV_Wrap.jpg")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, f"{source}.gltf")
    with open(out, "w") as f:
        json.dump(doc, f)
    return out


def import_prop(source, folder, name):
    if not os.path.isfile(os.path.join(PROJECT_DIR, "Assets", f"{source}.gltf")):
        unreal.log_warning(f"[LFF props] missing Assets/{source}.gltf; skipped")
        return None
    path = unique_materials(source)
    dest = f"{ROOT}/{folder}"
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.destination_name = name
    p = pipeline()
    params.override_pipelines = [unreal.SoftObjectPath(p.get_path_name())]
    unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(
        dest, unreal.InterchangeManager.create_source_data(path), params)
    meshes = [a for a in unreal.EditorAssetLibrary.list_assets(dest, recursive=False)
              if unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name == "StaticMesh"]
    unreal.EditorAssetLibrary.save_directory(dest)
    if source == "sensoryVehicle" and os.path.isfile(os.path.join(PROJECT_DIR, "Saved", "PropImport", "T_LFF_SAV_Wrap.jpg")):
        refinement = runpy.run_path(os.path.join(PROJECT_DIR, "Tools", "Editor", "refine_sensory_spaces.py"))
        refinement["apply_trailer_wrap"]()
    return meshes


def main():
    for source, (folder, name) in PROPS.items():
        meshes = import_prop(source, folder, name)
        for m in meshes or []:
            sm = unreal.load_asset(m)
            box = sm.get_bounding_box()
            size = box.max - box.min
            unreal.log(f"[LFF props] {m}: {size.x:.0f} x {size.y:.0f} x {size.z:.0f} cm, "
                       f"{len(sm.static_materials)} material slots")


if __name__ == "__main__":
    main()
