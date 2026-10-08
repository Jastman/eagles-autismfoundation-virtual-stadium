"""Builds /Game/LFF/UI/Materials/M_LFF_TimelineArc, the timeline arc's UI material, from
SourceData/UI/Materials/LFF_TimelineArc.hlsl (the Custom node's code is read from that file, so the two never drift).

Run inside the editor (Python), e.g. through the MCP execute_python_code tool:
    import runpy; runpy.run_path(r"<repo>/Tools/Editor/build_m_lff_timeline_arc.py")
Safe to rerun: it clears and rebuilds the graph of an existing asset in place, so WBP_LFF_TimelineArc keeps its reference.

The graph: TexCoord[0] and one parameter per input pin -> Custom node "ArcHUD" (Float 4) -> RGB to Final Color (the UI
domain's Emissive Color), A to Opacity. Parameter names and defaults match ULFFTimelineArc / FLFFArcShape, which set
them at runtime.
"""
import os

import unreal

ASSET_DIR = "/Game/LFF/UI/Materials"
ASSET_NAME = "M_LFF_TimelineArc"
HLSL = os.path.join(unreal.Paths.project_dir(), "SourceData", "UI", "Materials", "LFF_TimelineArc.hlsl")

# Pin order matches the HLSL file's list. "uv" is TexCoord[0]; scalars and colours become parameters of the same name.
SCALARS = [
    ("AspectRatio", 820.0 / 168.0),
    ("ArcCenterV", 0.36),
    ("CurveIntensity", 0.34),
    ("ArcThickness", 0.024),
    ("FadeBoundaryWidth", 0.12),
    ("BandAbove", 0.10),
    ("BandBelow", 0.62),
    ("BandSoftness", 0.06),
    ("BandOpacity", 0.94),
]
COLOURS = [
    ("BandColor", (0.0021, 0.0232, 0.0284)),  # SURFACE #072A2F
]
AFTER_BAND = [("ProgressU", 0.5)]
LINE_COLOURS = [
    ("DoneColor", (0.905, 0.888, 0.838)),  # INK
    ("AheadColor", (0.479, 0.558, 0.565)),  # INK_MUTED
]
LAST = [("AheadOpacity", 0.55)]
PINS = ["UV"] + [n for n, _ in SCALARS] + [n for n, _ in COLOURS] + [n for n, _ in AFTER_BAND] + [n for n, _ in LINE_COLOURS] + [n for n, _ in LAST]


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
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("description", "ArcHUD")
    custom.set_editor_property("code", open(HLSL, encoding="utf-8").read())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in PINS:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)

    y = -600
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -700, y)
    connect(uv, "", custom, "UV")
    for name, value in SCALARS + AFTER_BAND + LAST:
        y += 80
        node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        connect(node, "", custom, name)
    for name, (r, g, b) in COLOURS + LINE_COLOURS:
        y += 120
        node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(r, g, b, 1.0))
        connect(node, "", custom, name)

    # Every channel set explicitly: a new ComponentMask starts with R and G ticked, and an alpha mask that still had them
    # would feed R to Opacity (the band, whose red is near 0, would vanish and the end fade would do nothing).
    rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, -60)
    alpha = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, 60)
    for mask, channels in ((rgb, "rgb"), (alpha, "a")):
        for channel in "rgba":
            mask.set_editor_property(channel, channel in channels)
    connect(custom, "", rgb, "")
    connect(custom, "", alpha, "")
    for node, prop in ((rgb, unreal.MaterialProperty.MP_EMISSIVE_COLOR), (alpha, unreal.MaterialProperty.MP_OPACITY)):
        if not mel.connect_material_property(node, "", prop):
            raise RuntimeError(f"couldn't connect to {prop}")

    # An unconnected Custom input silently reads 0 (no band, a hairline), so check every pin before saving.
    connected = mel.get_inputs_for_material_expression(material, custom)
    missing = [PINS[i] for i, source in enumerate(connected) if source is None]
    if len(connected) != len(PINS) or missing:
        raise RuntimeError(f"Custom node pins: {len(connected)} of {len(PINS)}, unconnected: {missing}")

    # delete_all_material_expressions can leave nodes behind on a rebuild; drop anything not wired to an output.
    mel.delete_unused_expressions(material)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path)
    print(f"{path}: Custom node with {len(PINS)} pins, {mel.get_num_material_expressions(material)} expressions; saved.")


main()
