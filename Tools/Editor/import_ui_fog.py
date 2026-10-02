"""Imports the HUD's soft backgrounds (Tools/Pipeline/prepare_ui_fog.py) into /Game/LFF/UI/Icons as UI textures.

Run inside the editor (Python), e.g. through the MCP execute_python_code tool:
  import runpy; runpy.run_path(r"<repo>/Tools/Editor/import_ui_fog.py")
"""
import os

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NAMES = ("T_LFF_Fog", "T_LFF_FadeTop", "T_LFF_FadeBottom", "T_LFF_FadeSide")

tasks = []
for name in NAMES:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(REPO, "SourceData", "UI", name + ".png")
    task.destination_path = "/Game/LFF/UI/Icons"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in NAMES:
    tex = unreal.load_asset(f"/Game/LFF/UI/Icons/{name}")
    # Sharp, never streamed, full alpha: the same settings as the other UI textures.
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    print("imported", tex.get_path_name())
