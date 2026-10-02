"""Imports the journey table, sounds, voiceover, ticket frames and the eagle (Docs/ManualSetup.md §4). Safe to rerun.

  SourceData/Journey/LFF_GoldenPath.json  ->  /Game/LFF/Data/DT_LFF_GoldenPath
  SourceData/Audio/*.wav                  ->  /Game/LFF/Audio/  (the beds loop; the preview plays once)
  SourceData/Audio/VO/VO_*.wav            ->  /Game/LFF/Audio/VO/  (Tools/Editor/import_lff_vo.py; never loop)
  Saved/TicketImport/T_LFF_Ticket_*.png   ->  /Game/LFF/UI/Ticket/  (Tools/Pipeline/prepare_ticket.py)
  Saved/EagleImport/SK_LFF_BaldEagle.abc  ->  /Game/LFF/Eagle/  (GeometryCache + textures + material instances;
                                              Tools/Pipeline/prepare_eagle.py)
  /Game/LFF/Materials/M_LFF_Glow + MI_LFF_FireworkGlow (a steady unlit emissive for the anthem fireworks)
  Saved/LogoImport/T_LFF_Logo_*.png       ->  /Game/LFF/Logos/  (+ MI_LFF_Logo_*, masked; Tools/Pipeline/prepare_logos.py)

Make the sources first (Tools/Audio/make_lff_audio.py, make_vo_script.py; Tools/Pipeline/prepare_*.py). Parts whose
sources are missing are skipped with a warning. Run with the editor closed:
  Tools/Editor/run_headless.sh Tools/Editor/import_lff_content.py "LFF content"
  LFF_CONTENT_STEPS="ticket logos" Tools/Editor/run_headless.sh Tools/Editor/import_lff_content.py "LFF content"
"""
import glob
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
from import_lff_vo import import_vo  # noqa: E402

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
JOURNEY_JSON = os.path.join(PROJECT_DIR, "SourceData", "Journey", "LFF_GoldenPath.json")
AUDIO_SRC = os.path.join(PROJECT_DIR, "SourceData", "Audio")
TICKET_SRC = os.path.join(PROJECT_DIR, "Saved", "TicketImport")
EAGLE_SRC = os.path.join(PROJECT_DIR, "Saved", "EagleImport")
LOGO_SRC = os.path.join(PROJECT_DIR, "Saved", "LogoImport")
TABLE_DIR, TABLE_NAME = "/Game/LFF/Data", "DT_LFF_GoldenPath"
AUDIO_DIR = "/Game/LFF/Audio"
TICKET_DIR = "/Game/LFF/UI/Ticket"
EAGLE_DIR = "/Game/LFF/Eagle"
LOGO_DIR = "/Game/LFF/Logos"
MAT_DIR = "/Game/LFF/Materials"
MASTER, MASTER_MASKED = "/Game/LFF/Stadium/Materials/M_LFF_Stadium", "/Game/LFF/Stadium/Materials/M_LFF_StadiumMasked"
LOOPING = {"SW_LFF_AmbientPlaza", "SW_LFF_CheerBed"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"[LFF content] {msg}")


def task_for(path, dest, name=None, options=None, factory=None):
    task = unreal.AssetImportTask()
    for prop, value in (("filename", path), ("destination_path", dest),
                        ("destination_name", name or os.path.splitext(os.path.basename(path))[0]),
                        ("automated", True), ("save", False), ("replace_existing", True)):
        task.set_editor_property(prop, value)
    if options is not None:
        task.set_editor_property("options", options)
    if factory is not None:
        task.set_editor_property("factory", factory)
    return task


def import_table():
    row_struct = unreal.load_object(None, "/Script/VirtualStadium.LFFJourneyStep")
    # In UE 5.8 only the reimport subclass registers the .json format; it imports new tables too.
    factory = unreal.ReimportDataTableFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
    settings.set_editor_property("import_row_struct", row_struct)
    task = task_for(JOURNEY_JSON, TABLE_DIR, TABLE_NAME, factory=factory)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    table = unreal.load_asset(f"{TABLE_DIR}/{TABLE_NAME}")
    if not table:
        raise RuntimeError("DT_LFF_GoldenPath import failed")
    rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    log(f"imported {TABLE_NAME}: {len(rows)} rows {rows}")


def import_audio():
    files = sorted(glob.glob(os.path.join(AUDIO_SRC, "*.wav")))
    if not files:
        raise RuntimeError(f"no .wav files in {AUDIO_SRC}; run Tools/Audio/make_lff_audio.py first")
    tools.import_asset_tasks([task_for(path, AUDIO_DIR) for path in files])
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        wave = unreal.load_asset(f"{AUDIO_DIR}/{name}")
        if not isinstance(wave, unreal.SoundWave):
            raise RuntimeError(f"{name} did not import as a SoundWave")
        wave.set_editor_property("looping", name in LOOPING)
        eal.save_loaded_asset(wave)
        log(f"imported {name}: {wave.get_editor_property('duration'):.1f} s, looping={name in LOOPING}")


def ui_texture(tex):
    for prop, value in (("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
                        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
                        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
                        ("never_stream", True), ("srgb", True)):
        tex.set_editor_property(prop, value)
    eal.save_loaded_asset(tex)


def import_ticket():
    files = sorted(glob.glob(os.path.join(TICKET_SRC, "T_LFF_Ticket_*.png")))
    if not files:
        log(f"WARNING no ticket frames in {TICKET_SRC}; run Tools/Pipeline/prepare_ticket.py (the phone stays empty)")
        return
    tools.import_asset_tasks([task_for(path, TICKET_DIR) for path in files])
    for path in files:
        tex = unreal.load_asset(f"{TICKET_DIR}/{os.path.splitext(os.path.basename(path))[0]}")
        if tex is None:
            raise RuntimeError(f"{path} did not import")
        ui_texture(tex)
    log(f"imported {len(files)} ticket frames to {TICKET_DIR}")


def material_instance(name, parent_path, folder=MAT_DIR):
    path = f"{folder}/{name}"
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset(parent_path))
    return mi


def make_glow():
    """M_LFF_Glow: unlit, emissive = BaseTint x EmissiveStrength, flagged for instanced meshes (the firework sparks
    are one ISM per burst; without the flag the default material draws them black). MI_LFF_FireworkGlow is its
    instance; the fireworks actor sets the colour and strength per burst."""
    path = f"{MAT_DIR}/M_LFF_Glow"
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "M_LFF_Glow", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    mel.delete_all_material_expressions(m)
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 0)
    tint.set_editor_property("parameter_name", "BaseTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    strength = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 200)
    strength.set_editor_property("parameter_name", "EmissiveStrength")
    strength.set_editor_property("default_value", 1.0)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(tint, "", mul, "A")
    mel.connect_material_expressions(strength, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    mi = material_instance("MI_LFF_FireworkGlow", path)
    mel.set_material_instance_vector_parameter_value(mi, "BaseTint", unreal.LinearColor(1, 1, 1, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 4000.0)
    eal.save_loaded_asset(mi)
    log("made M_LFF_Glow and MI_LFF_FireworkGlow (unlit, instanced, steady emissive, no flicker)")


def import_eagle():
    abc = os.path.join(EAGLE_SRC, "SK_LFF_BaldEagle.abc")
    info_path = os.path.join(EAGLE_SRC, "eagle.json")
    if not os.path.exists(abc) or not os.path.exists(info_path):
        log(f"WARNING no eagle in {EAGLE_SRC}; run Tools/Pipeline/prepare_eagle.py (the anthem preview has fireworks only)")
        return
    with open(info_path) as f:
        info = json.load(f)

    # Textures, then one material instance per source material.
    textures = {}
    for mat_name, m in info["materials"].items():
        if m.get("texture") and os.path.exists(m["texture"]):
            tex_name = os.path.splitext(os.path.basename(m["texture"]))[0]
            if tex_name not in textures:
                tools.import_asset_tasks([task_for(m["texture"], f"{EAGLE_DIR}/Textures", tex_name)])
                tex = unreal.load_asset(f"{EAGLE_DIR}/Textures/{tex_name}")
                eal.save_loaded_asset(tex)
                textures[tex_name] = tex

    # Blender writes metres, Y-up; Unreal wants cm, Z-up (the Maya preset flips the axes; the x100 makes cm).
    options = unreal.AbcImportSettings()
    options.set_editor_property("import_type", unreal.AlembicImportType.GEOMETRY_CACHE)
    conv = options.get_editor_property("conversion_settings")
    conv.set_editor_property("preset", unreal.AbcConversionPreset.MAYA)
    conv.set_editor_property("scale", unreal.Vector(100.0, -100.0, 100.0))
    conv.set_editor_property("rotation", unreal.Vector(90.0, 0.0, 0.0))
    options.set_editor_property("conversion_settings", conv)
    gc = options.get_editor_property("geometry_cache_settings")
    for prop, value in (("flatten_tracks", True), ("apply_constant_topology_optimizations", False)):
        try:
            gc.set_editor_property(prop, value)
        except Exception as e:  # noqa: BLE001 - property names differ between engine versions
            log(f"geometry cache setting {prop} not set: {e}")
    options.set_editor_property("geometry_cache_settings", gc)
    sampling = options.get_editor_property("sampling_settings")
    sampling.set_editor_property("frame_start", 0)
    sampling.set_editor_property("frame_end", int(info["frames"]) - 1)
    options.set_editor_property("sampling_settings", sampling)
    tools.import_asset_tasks([task_for(abc, EAGLE_DIR, "GC_LFF_BaldEagle", options=options)])
    cache = unreal.load_asset(f"{EAGLE_DIR}/GC_LFF_BaldEagle")
    if cache is None:
        raise RuntimeError("the eagle geometry cache did not import")

    slots = list(cache.get_editor_property("materials"))
    names = []
    try:
        names = [str(n) for n in cache.get_editor_property("material_slot_names")]
    except Exception:  # noqa: BLE001
        pass
    log(f"eagle geometry cache: {len(slots)} material slots {names}")
    assigned = []
    for i in range(len(slots)):
        slot_name = names[i] if i < len(names) else ""
        mat_name = next((k for k in info["materials"] if k and (k in slot_name or slot_name in k.replace(" ", "_"))), None)
        if mat_name is None and i < len(info["materials"]):
            mat_name = list(info["materials"])[i]
        m = info["materials"].get(mat_name, {})
        tex = textures.get(os.path.splitext(os.path.basename(m.get("texture") or ""))[0])
        masked = bool(tex) and (m.get("texture") or "").endswith(".png")
        mi = material_instance(f"MI_LFF_Eagle_{i}", MASTER_MASKED if masked else MASTER, f"{EAGLE_DIR}/Materials")
        if tex is not None:
            mel.set_material_instance_texture_parameter_value(mi, "BaseColorMap", tex)
        else:
            tint = m.get("tint", [0.05, 0.05, 0.05])
            mel.set_material_instance_vector_parameter_value(mi, "BaseTint", unreal.LinearColor(*tint[:3], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.8)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", 0.0)
        mel.set_material_instance_scalar_parameter_value(mi, "Specular", 0.3)
        eal.save_loaded_asset(mi)
        slots[i] = mi
        assigned.append(f"{slot_name or i}->{mat_name}")
    cache.set_editor_property("materials", slots)
    eal.save_loaded_asset(cache)
    log(f"imported GC_LFF_BaldEagle: {info['frames']} frames ({info['loop_seconds']:.1f} s loop), materials {assigned}")


def import_logos():
    """The logos from Assets/Logos (prepare_logos.py), each with a masked, two-sided instance of the stadium master:
    the logo's alpha cuts it out, so it sits on a sign plate with no box around it."""
    files = sorted(glob.glob(os.path.join(LOGO_SRC, "T_LFF_Logo_*.png")))
    if not files:
        log(f"WARNING no logos in {LOGO_SRC}; run Tools/Pipeline/prepare_logos.py (the finale sign stays blank)")
        return
    tools.import_asset_tasks([task_for(path, LOGO_DIR) for path in files])
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        tex = unreal.load_asset(f"{LOGO_DIR}/{name}")
        if tex is None:
            raise RuntimeError(f"{path} did not import")
        tex.set_editor_property("srgb", True)
        eal.save_loaded_asset(tex)
        mi = material_instance(name.replace("T_LFF_", "MI_LFF_"), MASTER_MASKED, LOGO_DIR)
        mel.set_material_instance_texture_parameter_value(mi, "BaseColorMap", tex)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.85)  # printed vinyl, no glare
        mel.set_material_instance_scalar_parameter_value(mi, "Specular", 0.3)
        eal.save_loaded_asset(mi)
        log(f"imported {name} and its material instance to {LOGO_DIR}")

    # The midfield logo is a decal, so it follows the turf and can't z-fight with it from the low seat views.
    path = f"{LOGO_DIR}/M_LFF_LogoDecal"
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "M_LFF_LogoDecal", LOGO_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mel.delete_all_material_expressions(m)
    logo = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    logo.set_editor_property("parameter_name", "Logo")
    logo.set_editor_property("texture", unreal.load_asset(f"{LOGO_DIR}/T_LFF_Logo_Eagles"))
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 250)
    tint.set_editor_property("parameter_name", "BaseTint")
    # Turf paint under stadium sun reads darker than the print file; 0.8 keeps the whites off clipping.
    tint.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.8, 1))
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 50)
    mel.connect_material_expressions(logo, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(logo, "A", unreal.MaterialProperty.MP_OPACITY)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", 0.9)  # painted turf, no sheen
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    mi = material_instance("MI_LFF_LogoDecal_Eagles", path, LOGO_DIR)
    mel.set_material_instance_texture_parameter_value(mi, "Logo", unreal.load_asset(f"{LOGO_DIR}/T_LFF_Logo_Eagles"))
    eal.save_loaded_asset(mi)
    log("made M_LFF_LogoDecal and MI_LFF_LogoDecal_Eagles (the midfield logo)")


STEPS = [("table", import_table), ("audio", import_audio), ("vo", lambda: import_vo(log)), ("ticket", import_ticket),
         ("glow", make_glow), ("eagle", import_eagle), ("logos", import_logos)]

try:
    # Steps by name, from the arguments or LFF_CONTENT_STEPS (the project path has spaces, so arguments after
    # -ExecutePythonScript don't survive run_headless.sh): LFF_CONTENT_STEPS="ticket logos" run_headless.sh ...
    only = [a for a in sys.argv[1:] if not a.startswith("-")] or os.environ.get("LFF_CONTENT_STEPS", "").split()
    for name, step in STEPS:
        if not only or name in only:
            step()
    log("DONE")
except Exception as e:  # noqa: BLE001 - report and let run_headless.sh stop the editor
    unreal.log_error(f"[LFF content] FAILED: {e}")
