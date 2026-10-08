"""Builds /Game/LFF/UI/Materials/M_LFF_LevelHighlight, Free explore's level highlight (a post-process material), from
SourceData/UI/Materials/LFF_LevelHighlight.hlsl (the Custom node's code is read from that file, so the two never drift).

Run inside the editor (Python), e.g. through the MCP execute_python_code tool (build_wbp_lff_modes.py does this):
    import runpy; runpy.run_path(r"<repo>/Tools/Editor/build_m_lff_level_highlight.py")
Safe to rerun: an existing asset's graph is cleared and rebuilt in place. Fails if any Custom node pin is unconnected.
"""
import os

import unreal

ASSET_DIR = "/Game/LFF/UI/Materials"
ASSET_NAME = "M_LFF_LevelHighlight"
HLSL = os.path.join(unreal.Paths.project_dir(), "SourceData", "UI", "Materials", "LFF_LevelHighlight.hlsl")

VECTORS = [
    ("SiteOrigin", (0.0, 0.0, 0.0)),
    ("SiteAxisX", (1.0, 0.0, 0.0)),
    ("SiteAxisY", (0.0, 1.0, 0.0)),
    ("SiteAxisZ", (0.0, 0.0, 1.0)),
    ("HighlightColor", (0.141, 0.514, 0.021)),  # KELLY #69BE28, linear
]
SCALARS = [
    ("BandMin", 120.0),
    ("BandMax", 1000.0),
    ("FootprintX", 13000.0),
    ("FootprintY", 14000.0),
    ("HighlightOpacity", 0.35),
    ("DimOutside", 0.3),
]
PINS = ["SceneColor", "WorldPos"] + [n for n, _ in VECTORS[:4]] + ["BandMin", "BandMax", "FootprintX", "FootprintY", "HighlightColor",
                                                                 "HighlightOpacity", "DimOutside"]


def connect(source, source_output, target, target_input):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(source, source_output, target, target_input):
        raise RuntimeError(f"couldn't connect {source.get_name()} to {target_input or target.get_name()}")


def main():
    mel = unreal.MaterialEditingLibrary
    path = f"{ASSET_DIR}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, ASSET_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("description", "LevelHighlight")
    custom.set_editor_property("code", open(HLSL, encoding="utf-8").read())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in PINS:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)

    scene = mel.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -800, -400)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    connect(scene, "Color", custom, "SceneColor")
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -800, -300)
    connect(world, "", custom, "WorldPos")

    y = -200
    for name, (r, g, b) in VECTORS:
        y += 110
        node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(r, g, b, 0.0))
        connect(node, "", custom, name)
    for name, value in SCALARS:
        y += 80
        node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        connect(node, "", custom, name)

    if not mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("couldn't connect to Emissive Color")
    connected = mel.get_inputs_for_material_expression(material, custom)
    missing = [PINS[i] for i, source in enumerate(connected) if source is None]
    if len(connected) != len(PINS) or missing:
        raise RuntimeError(f"Custom node pins: {len(connected)} of {len(PINS)}, unconnected: {missing}")

    mel.delete_unused_expressions(material)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path)
    print(f"{path}: Custom node with {len(PINS)} pins, {mel.get_num_material_expressions(material)} expressions; saved.")


main()
