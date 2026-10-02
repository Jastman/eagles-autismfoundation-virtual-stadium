"""Session 5 level and asset pass, headless (the editor must be closed):
  Tools/Editor/run_headless.sh Tools/Editor/session5_assets.py "LFF s5"

Inputs (make them first):
  Saved/LotPlan/plan.json            Tools/Pipeline/plan_lot.py      lot cars, paint lines, hero bays, parking camera, queue
  Content/LFF/Crowd/seats.json       Tools/Pipeline/extract_seats.py the stadium's 56,707 seats (staged with the build)
  SourceData/Audio/VO/*.words.json   Tools/Audio/make_vo_words.py    karaoke word timings
  Saved/IconImport/*.png             Tools/Pipeline/prepare_icons.py the card's voice button icons

What it does (safe to rerun: everything it placed carries the tag LFF_S5 and is removed first):
  - The lot on its own grid: SI_LotCars (instanced City Sample cars), SI_LotPaint (white stall and centre lines), the ten
    hero cars and the arriving car moved into their bays, CT_Parking in the aisle looking south at the stadium, the
    parking sign over the rows. The old bay lines (SI_ParkingBays) and the session 4 lot are removed.
  - The crowd: the MetaHuman Crowd Sample, by Mass. DA_LFF_StadiumCrowd is the sample's base crowd config without the
    walking, avoidance and navmesh traits (people stand where they spawn) and with its far LOD carried out to 600 m, so
    the aerial still shows them. Two ALFFStadiumCrowdSpawners: the bowl (the seats) and the queue at the Pepsi Plaza
    entry (Content/LFF/Crowd/queue.json, written here with traced heights). The session 4 crowd cards and the six
    primitive queue people are removed or hidden.
  - Each voice SoundWave gets its word timings as subtitle cues (suppressed on screen; the card highlights the word).
  - The voice button icons, as UI textures.
Local coordinates are the stadium frame; LFF_SiteRoot sits at the world origin turned -6.5 degrees.
"""
import json
import math
import os

import unreal

TAG = "LFF_S5"
LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SITE_DEG = -6.5
CROWD_DIR = "/Game/LFF/Crowd"
MAT_DIR = "/Game/LFF/Materials"
CROWD_CONFIG = f"{CROWD_DIR}/DA_LFF_StadiumCrowd"
BASE_CONFIG = "/MetaHumanCrowdContent/AI/DA_MHMassCrowdConfig_Base"
CITY_CARS = ("02", "03", "05", "06", "07")
PAINT_MIX = {"White": 22, "Black": 18, "Silver": 18, "Grey": 16, "Navy": 8, "Red": 7, "Beige": 6, "Green": 5}
CAR_PAINTS = {"White": (0.70, 0.70, 0.68), "Black": (0.012, 0.012, 0.014), "Silver": (0.36, 0.37, 0.38),
              "Grey": (0.12, 0.125, 0.13), "Navy": (0.012, 0.03, 0.09), "Red": (0.30, 0.015, 0.012),
              "Beige": (0.42, 0.36, 0.26), "Green": (0.01, 0.07, 0.06)}
# Traits the bowl and queue don't need: they stand still. (Walking, steering, avoidance and navmesh queries would
# push neighbours 60 cm apart off their seats.)
DROP_TRAITS = ("MassNavMeshNavigationTrait", "MassSteeringTrait", "MassSmartObjectUserTrait", "MassNavigationObstacleTrait",
               "MassObstacleAvoidanceTrait", "MassSmoothOrientationTrait")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"[LFF s5] {msg}")


def world_xy(x, y):
    c, s = math.cos(math.radians(SITE_DEG)), math.sin(math.radians(SITE_DEG))
    return x * c - y * s, x * s + y * c


def actors():
    return {a.get_actor_label(): a for a in eas.get_all_level_actors()}


def site_root():
    return next(a for a in eas.get_all_level_actors() if a.actor_has_tag("LFF_SiteRoot"))


def add_component(actor, cls, name):
    handles = sds.k2_gather_subobject_data_for_instance(actor)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=None)
    handle, fail = sds.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f"add {cls} to {actor.get_actor_label()}: {fail}")
    sds.rename_subobject(handle, unreal.Text(name))
    return actor.get_components_by_class(cls)[-1]


def spawn(cls, label, loc=(0, 0, 0), yaw=0.0, folder="LFF_SiteRoot/LFF_StandIns", tags=()):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.tags = [unreal.Name(t) for t in (TAG, *tags)]
    return a


class Ground:
    """Line traces onto the model (complex collision), ignoring the cars."""

    def __init__(self, ignore):
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        self.ignore = ignore

    def z(self, x, y, top=900.0, depth=1500.0):
        wx, wy = world_xy(x, y)
        hit = unreal.SystemLibrary.line_trace_single(self.world, unreal.Vector(wx, wy, top), unreal.Vector(wx, wy, top - depth),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, self.ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        t = hit.to_tuple()
        return t[5].z if t[0] else None


# --- assets --------------------------------------------------------------------------------------------------------
def ui_texture(path):
    tex = unreal.load_asset(path)
    for prop, value in (("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
                        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
                        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
                        ("never_stream", True), ("srgb", True)):
        tex.set_editor_property(prop, value)
    eal.save_loaded_asset(tex)


def import_icons():
    folder = "/Game/LFF/UI/Icons"
    tasks = []
    for name in ("T_LFF_Icon_VoiceOn", "T_LFF_Icon_VoiceOff"):
        task = unreal.AssetImportTask()
        for prop, value in (("filename", os.path.join(PROJECT, "Saved", "IconImport", f"{name}.png")), ("destination_path", folder),
                            ("destination_name", name), ("automated", True), ("save", False), ("replace_existing", True)):
            task.set_editor_property(prop, value)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    task = unreal.AssetImportTask()
    for prop, value in (("filename", os.path.join(PROJECT, "Saved", "LogoImport", "T_LFF_Logo_EAF.png")), ("destination_path", folder),
                        ("destination_name", "T_LFF_Logo_EAF_UI"), ("automated", True), ("save", False), ("replace_existing", True)):
        task.set_editor_property(prop, value)
    tools.import_asset_tasks([task])
    for name in ("T_LFF_Icon_VoiceOn", "T_LFF_Icon_VoiceOff", "T_LFF_Logo_EAF_UI"):
        ui_texture(f"{folder}/{name}")
    log("icons and the finale wordmark imported as UI textures")


def voice_words():
    vo = os.path.join(PROJECT, "SourceData", "Audio", "VO")
    n = 0
    for f in sorted(os.listdir(vo)):
        if not f.endswith(".words.json"):
            continue
        data = json.load(open(os.path.join(vo, f), encoding="utf-8"))
        wave = unreal.load_asset(f"/Game/LFF/Audio/VO/{data['wave']}")
        if wave is None:
            log(f"WARNING no SoundWave {data['wave']}")
            continue
        cues = []
        for w in data["words"]:
            cue = unreal.SubtitleCue()
            cue.set_editor_property("text", w["w"])
            cue.set_editor_property("time", float(w["t"]))
            cues.append(cue)
        wave.set_editor_property("subtitles", cues)
        eal.save_loaded_asset(wave)
        n += 1
    log(f"word cues on {n} voice lines")


def ism_usage(material_interface):
    """The base material of an instanced mesh's material needs the ISM usage flag (or cooked builds draw nothing)."""
    base = material_interface.get_base_material() if material_interface else None
    # Only the project's own materials: engine content is read-only (a slot with no material resolves to the engine's
    # default material, which renders on any mesh type anyway).
    if base and base.get_path_name().startswith("/Game/") and not base.get_editor_property("used_with_instanced_static_meshes"):
        base.set_editor_property("used_with_instanced_static_meshes", True)
        eal.save_loaded_asset(base)
        log(f"ISM usage flag set on {base.get_path_name()}")


def lot_paint_material():
    path = f"{MAT_DIR}/M_LFF_LotPaint"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_LFF_LotPaint", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    c.set_editor_property("constant", unreal.LinearColor(0.62, 0.62, 0.58, 1))  # worn white road paint
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 200)
    r.set_editor_property("r", 0.85)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def car_paint(model, colour, cache={}):
    key = (model, colour)
    if key in cache:
        return cache[key]
    body = unreal.load_asset(f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh/SM_vehCar_vehicle{model}_No_Wheel")
    slots = [str(s.material_slot_name) for s in body.static_materials]
    index = slots.index("veh_carPaint")
    name = f"MI_LFF_CarPaint_{model}_{colour}"
    path = f"{MAT_DIR}/Cars/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, f"{MAT_DIR}/Cars", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", body.static_materials[index].material_interface)
        mel.set_material_instance_static_switch_parameter_value(mi, "Paint Variation", False)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(*CAR_PAINTS[colour], 1))
        eal.save_loaded_asset(mi)
    cache[key] = (index, mi)
    return cache[key]


def crowd_config():
    if not eal.does_asset_exist(CROWD_CONFIG):
        if not eal.duplicate_asset(BASE_CONFIG, CROWD_CONFIG):
            raise RuntimeError(f"could not duplicate {BASE_CONFIG}")
    asset = unreal.load_asset(CROWD_CONFIG)
    cfg = asset.get_editor_property("config")
    traits = list(cfg.get_editor_property("traits"))
    kept = [t for t in traits if not any(d in t.get_class().get_name() for d in DROP_TRAITS)]
    cfg.set_editor_property("traits", kept)
    vis = next(t for t in kept if "MetaHumanMassCrowdVisualizationTrait" in t.get_class().get_name())
    lod = vis.get_editor_property("lod_params")
    # High-res actors for the nearest few, then instanced skinned meshes out to 600 m (the aerial), no hard cap.
    for prop, value in (("base_lod_distance", [0.0, 500.0, 1000.0, 60000.0]), ("visible_lod_distance", [0.0, 1000.0, 5000.0, 60000.0]),
                        ("lod_max_count", [5, 0, 100000, 2147483647])):
        try:
            lod.set_editor_property(prop, value)
        except Exception as e:
            log(f"WARNING could not set LOD {prop}: {e}")
    vis.set_editor_property("lod_params", lod)
    # Instanced skinned meshes at every distance: the sample's actor representations (BP_MassHighRes) are driven by
    # the walking traits dropped above, and without them stand in A pose at the capsule's height.
    params = vis.get_editor_property("params")
    params.set_editor_property("lod_representation", [unreal.MassRepresentationType.SKINNED_MESH_INSTANCE] * 3 + [unreal.MassRepresentationType.NONE])
    vis.set_editor_property("params", params)
    asset.set_editor_property("config", cfg)
    eal.save_loaded_asset(asset)
    log(f"{CROWD_CONFIG}: traits {[t.get_class().get_name() for t in kept]}; LOD {list(lod.get_editor_property('visible_lod_distance'))}")
    return asset


# --- level ---------------------------------------------------------------------------------------------------------
def remove_previous():
    gone = 0
    for a in eas.get_all_level_actors():
        label = a.get_actor_label()
        if a.actor_has_tag(TAG) or label.startswith("ZZ_") or label in ("SI_LotCars", "SI_BowlCrowd", "SI_ParkingBays"):
            eas.destroy_actor(a)
            gone += 1
    log(f"removed {gone} old actors (test actors, the session 4 lot and crowd cards, the old bay lines)")


def place_lot(plan, ground):
    rng_colours = list(PAINT_MIX)
    weights = [PAINT_MIX[c] for c in rng_colours]
    import random
    rng = random.Random(1019)
    groups = {}
    misses = 0
    for x, y, yaw in plan["cars"]:
        z = ground.z(x, y)
        if z is None or not 120 <= z <= 170:
            misses += 1
            continue
        model = rng.choice(CITY_CARS)
        colour = rng.choices(rng_colours, weights)[0]
        groups.setdefault((model, colour), []).append(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw), unreal.Vector(1, 1, 1)))
    lot = spawn(unreal.Actor, "SI_LotCars", tags=("LFF_Exterior",))
    lot.attach_to_actor(site_root(), "", unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE,
                        unreal.AttachmentRule.KEEP_RELATIVE, False)
    lot.set_actor_relative_transform(unreal.Transform(), False, False)
    by_model = {}
    n_comp = 0
    for (model, colour), inst in sorted(groups.items()):
        by_model.setdefault(model, []).extend(inst)
        base = f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh"
        c = add_component(lot, unreal.InstancedStaticMeshComponent, f"Body_{model}_{colour}")
        mesh = unreal.load_asset(f"{base}/SM_vehCar_vehicle{model}_No_Wheel")
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_static_mesh(mesh)
        index, paint = car_paint(model, colour)
        c.set_material(index, paint)
        for s in mesh.static_materials:
            ism_usage(s.material_interface)
        ism_usage(paint)
        c.add_instances(inst, False, False)
        n_comp += 1
    for model, inst in sorted(by_model.items()):
        base = f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh"
        for part in [f"SM_Wheel_{w}_vehCar_vehicle{model}" for w in ("Front_L", "Front_R", "Rear_L", "Rear_R")] + [f"SM_All_Trans_vehCar_vehicle{model}"]:
            c = add_component(lot, unreal.InstancedStaticMeshComponent, part.replace(f"_vehCar_vehicle{model}", f"_{model}").replace("SM_", ""))
            mesh = unreal.load_asset(f"{base}/{part}")
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_static_mesh(mesh)
            for s in mesh.static_materials:
                ism_usage(s.material_interface)
            c.add_instances(inst, False, False)
            n_comp += 1
    log(f"lot: {sum(len(v) for v in groups.values())} cars ({misses} bays off the lot) in {n_comp} instanced components")
    ground.ignore = list(ground.ignore) + [lot]

    # Paint: thin white strips on the asphalt (2 cm proud of the traced lot).
    paint = spawn(unreal.Actor, "SI_LotPaint", tags=("LFF_Exterior",))
    paint.attach_to_actor(site_root(), "", unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE,
                          unreal.AttachmentRule.KEEP_RELATIVE, False)
    paint.set_actor_relative_transform(unreal.Transform(), False, False)
    c = add_component(paint, unreal.InstancedStaticMeshComponent, "Lines")
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    c.set_material(0, lot_paint_material())
    c.set_editor_property("cast_shadow", False)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    lines = []
    for x, y, yaw, length, width in plan["lines"]:
        z = ground.z(x, y)
        if z is None or not 120 <= z <= 170:
            continue
        lines.append(unreal.Transform(unreal.Vector(x, y, z + 1.0), unreal.Rotator(0, 0, yaw), unreal.Vector(length / 100, width / 100, 0.02)))
    c.add_instances(lines, False, False)
    log(f"lot paint: {len(lines)} lines")


def place_parking_step(plan, ground):
    a = actors()
    heroes = [a[f"SI_Car{i}"] for i in range(1, 11)]
    for actor, (x, y, yaw) in zip(heroes, plan["hero"]):
        wx, wy = world_xy(x, y)
        z = ground.z(x, y)
        actor.set_actor_location_and_rotation(unreal.Vector(wx, wy, z), unreal.Rotator(0, 0, yaw + SITE_DEG), False, False)
        if not actor.actor_has_tag("LFF_Exterior"):
            actor.tags = list(actor.tags) + [unreal.Name("LFF_Exterior")]
    x, y, yaw = plan["arrival"]
    car = a["SI_CarArrival"]
    wx, wy = world_xy(x, y)
    car.set_actor_location_and_rotation(unreal.Vector(wx, wy, ground.z(x, y)), unreal.Rotator(0, 0, yaw + SITE_DEG), False, False)
    car.set_editor_property("start_offset", unreal.Vector(*plan["arrival_start_offset"]))  # from the aisle, nose first
    car.set_editor_property("play_seconds", 4.0)

    cam = plan["camera"]
    gz = ground.z(cam["x"], cam["y"])
    wx, wy = world_xy(cam["x"], cam["y"])
    a["CT_Parking"].set_actor_location_and_rotation(unreal.Vector(wx, wy, gz + cam["eye"]),
                                                     unreal.Rotator(0, cam["pitch"], cam["yaw"] + SITE_DEG), False, False)
    sx, sy = plan["sign"]["pos"]
    fx, fy = plan["sign"]["face"]
    wx, wy = world_xy(sx, sy)
    face = math.degrees(math.atan2(fy - sy, fx - sx)) + SITE_DEG  # a TextRender reads from its +X side
    a["LBL_Parking"].set_actor_location_and_rotation(unreal.Vector(wx, wy, ground.z(sx, sy) + 520), unreal.Rotator(0, 0, face), False, False)
    log(f"parking step: 10 hero cars, the arriving car and CT_Parking (local {cam['x']:.0f}, {cam['y']:.0f}, yaw {cam['yaw']:.1f}) placed")


def place_crowd(plan, ground, config):
    # The queue's points with traced heights, in the seat list's format. Only the ground counts: not the stanchions,
    # the hidden primitive queue people (they still collide) or any other prop.
    ground.ignore = list(ground.ignore) + [a for a in eas.get_all_level_actors()
                                           if a.get_actor_label().startswith(("SI_", "LBL_", "TXT_"))]
    queue = []
    for x, y, _, yaw in plan["queue"]:
        z = ground.z(x, y, top=600)
        if z is not None:
            queue.append([x, y, round(z, 1), yaw])
    path = os.path.join(PROJECT, "Content", "LFF", "Crowd", "queue.json")
    with open(path, "w") as f:
        json.dump({"frame": "LFF_SiteRoot local, cm; [x, y, floor z, yaw deg]", "seats": queue}, f)

    for a in eas.get_all_level_actors():
        label = a.get_actor_label()
        if label.startswith("SI_CrowdLine"):
            a.set_actor_hidden_in_game(True)
            a.tags = [t for t in a.tags if str(t) != "LFF_Crowd"] + [unreal.Name("LFF_HiddenStandIn")]

    entity = unreal.MassSpawnedEntityType()
    entity.set_editor_property("entity_config", config)
    entity.set_editor_property("proportion", 1.0)
    # The queue is clothed ALFFPersonActors now (build_lff_standins.queue_people): the Mass queue stood in the bind pose.
    for label, seats, occupancy, clear in (("SI_StadiumCrowd", "LFF/Crowd/seats.json", 0.7, ["CT_SeatFieldView"]),):
        s = spawn(unreal.LFFStadiumCrowdSpawner, label, folder="LFF_SiteRoot/LFF_Crowd", tags=("LFF_Crowd",))
        s.set_editor_property("entity_types", [entity])
        s.set_editor_property("occupancy", occupancy)
        gen = s.get_editor_property("seat_generator")
        gen.set_editor_property("seats_file", seats)
        gen.set_editor_property("clear_around_targets", [unreal.Name(c) for c in clear])
        if not clear:
            gen.set_editor_property("front_offset", 0.0)
    log(f"crowd: bowl spawner (70% of the seats) and queue spawner ({len(queue)} people); six primitive queue people hidden")


def main():
    if not les.load_level(LEVEL):
        raise RuntimeError(f"could not load {LEVEL}")
    plan = json.load(open(os.path.join(PROJECT, "Saved", "LotPlan", "plan.json")))
    import_icons()
    voice_words()
    config = crowd_config()
    remove_previous()
    a = actors()
    ignore = [a[f"SI_Car{i}"] for i in range(1, 11)] + [a["SI_CarArrival"]]
    ground = Ground(ignore)
    place_lot(plan, ground)
    place_parking_step(plan, ground)
    place_crowd(plan, ground, config)
    if not les.save_current_level():
        raise RuntimeError("save_current_level failed")
    log("level saved")


try:
    main()
    log("DONE")
except Exception as e:
    import traceback
    unreal.log_error(f"[LFF s5] FAILED: {e}\n{traceback.format_exc()}")
finally:
    unreal.SystemLibrary.quit_editor()
