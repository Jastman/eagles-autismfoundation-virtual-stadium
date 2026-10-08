"""Apply the SAV photo wrap and refine the existing sensory room without moving its root or camera.

Prepare textures first with Tools/Pipeline/prepare_sav_wrap.py and prepare_sensory_room.py.
Run this script in the editor on L_LFF_Stadium. Reruns update only this refinement's assets/actors.
The room is an illustrative reconstruction, with static panels, not a surveyed interior.
"""
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ROOM_DIR = "/Game/LFF/Props/SensoryRoom"
TRAILER_DIR = "/Game/LFF/Props/SensoryVehicle"
TAG = "LFF_SensoryRefinement"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def require_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Missing required asset: {path}")
    return asset


def attach_room_actor(actor, room):
    # Static mobility detaches furnishings from the movable, georeferenced room.
    actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.root_component.set_absolute(False, False, False)
    rule = unreal.AttachmentRule.KEEP_RELATIVE
    if not actor.attach_to_actor(room, "", rule, rule, rule, False):
        raise RuntimeError(f"Could not attach {actor.get_actor_label()} to the sensory room")


def verify_room_attachments(room):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for actor in actors:
        if TAG not in [str(tag) for tag in actor.tags]:
            continue
        root = actor.root_component
        if (actor.get_attach_parent_actor() != room
                or root.mobility != unreal.ComponentMobility.MOVABLE
                or any(root.get_editor_property(key) for key in
                       ("absolute_location", "absolute_rotation", "absolute_scale"))):
            raise RuntimeError(f"Invalid sensory-room attachment: {actor.get_actor_label()}")


def import_texture(source, folder, name):
    if not source.is_file():
        raise RuntimeError(f"Missing texture: {source}")
    task = unreal.AssetImportTask()
    for key, value in (("filename", str(source)), ("destination_path", folder), ("destination_name", name),
                       ("automated", True), ("replace_existing", True), ("save", True)):
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = require_asset(f"{folder}/{name}")
    texture.set_editor_property("srgb", True)
    EAL.save_loaded_asset(texture)
    return texture


def apply_trailer_wrap():
    texture = import_texture(ROOT / "Saved/PropImport/T_LFF_SAV_Wrap.jpg", TRAILER_DIR, "T_LFF_SAV_Wrap")
    original_texture = require_asset(f"{TRAILER_DIR}/sensoryVehicle_texture_1")
    path = f"{TRAILER_DIR}/M_LFF_SAV_PhotoWrap"
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(
        f"{TRAILER_DIR}/trailer", path)
    if material is None:
        raise RuntimeError("Could not duplicate the trailer material")
    changed = 0
    for expression in MEL.get_material_expressions(material):
        if isinstance(expression, (unreal.MaterialExpressionTextureSample, unreal.MaterialExpressionTextureObject)):
            if expression.get_editor_property("texture") in (original_texture, texture):
                expression.set_editor_property("texture", texture)
                changed += 1
    if not changed:
        raise RuntimeError("The trailer material has no sample for its wrap texture")
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    mesh = require_asset(f"{TRAILER_DIR}/SM_LFF_SensoryVehicle")
    slots = [i for i, slot in enumerate(mesh.static_materials) if str(slot.material_slot_name) == "trailer"]
    if len(slots) != 1:
        raise RuntimeError(f"Expected one trailer material slot, found {slots}")
    index = slots[0]
    mesh.set_material(index, material)
    EAL.save_loaded_asset(mesh)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in subsystem.get_all_level_actors():
        if actor.get_actor_label() == "SI_SensoryTrailer":
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                if component.static_mesh == mesh:
                    component.set_material(index, material)
    unreal.log("[LFF sensory] Explicit photo wrap applied to the hull slot; other slots unchanged")


def master_material():
    path = f"{ROOM_DIR}/M_LFF_SensorySurface"
    if EAL.does_asset_exist(path):
        return require_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_LFF_SensorySurface", ROOM_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("Could not create sensory-room material")
    material.set_editor_property("two_sided", True)
    tex = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
    tex.set_editor_property("parameter_name", "Surface")
    tex.set_editor_property("texture", require_asset("/Engine/EngineResources/WhiteSquareTexture"))
    tint = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 200)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    multiply = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 0)
    glow = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 300)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 0.0)
    emissive = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 0, 200)
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 450)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.9)
    connections = [
        MEL.connect_material_expressions(tex, "RGB", multiply, "A"),
        MEL.connect_material_expressions(tint, "", multiply, "B"),
        MEL.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR),
        MEL.connect_material_expressions(multiply, "", emissive, "A"),
        MEL.connect_material_expressions(glow, "", emissive, "B"),
        MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR),
        MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS),
    ]
    if not all(connections):
        raise RuntimeError("Could not connect sensory-room material graph")
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    return material


def surface(name, tint, texture=None, glow=0, roughness=0.9):
    path = f"{ROOM_DIR}/MI_LFF_Sensory_{name}"
    instance = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        f"MI_LFF_Sensory_{name}", ROOM_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        raise RuntimeError(f"Could not create {path}")
    MEL.set_material_instance_parent(instance, master_material())
    expected_texture = texture or require_asset("/Engine/EngineResources/WhiteSquareTexture")
    MEL.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(*tint, 1))
    MEL.set_material_instance_scalar_parameter_value(instance, "Glow", glow)
    MEL.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
    MEL.set_material_instance_texture_parameter_value(instance, "Surface", expected_texture)
    MEL.update_material_instance(instance)
    actual_tint = MEL.get_material_instance_vector_parameter_value(instance, "Tint")
    if (any(abs(actual - expected) > 1e-5 for actual, expected in zip(
            (actual_tint.r, actual_tint.g, actual_tint.b), tint))
            or abs(MEL.get_material_instance_scalar_parameter_value(instance, "Glow") - glow) > 1e-5
            or abs(MEL.get_material_instance_scalar_parameter_value(instance, "Roughness") - roughness) > 1e-5
            or MEL.get_material_instance_texture_parameter_value(instance, "Surface") != expected_texture):
        raise RuntimeError(f"Material parameter verification failed: {path}")
    EAL.save_loaded_asset(instance)
    return instance


def refine_room():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    roots = [a for a in subsystem.get_all_level_actors() if a.get_actor_label() == "SI_SensoryRoomProxy"]
    if len(roots) != 1:
        raise RuntimeError(f"Expected one existing sensory room, found {len(roots)}")
    room = roots[0]
    bag_source = ROOT / "Saved/SensoryRoomImport/SM_LFF_SensoryBeanBag.gltf"
    if not bag_source.is_file():
        raise RuntimeError(f"Missing bean-bag mesh: {bag_source}")
    task = unreal.AssetImportTask()
    for key, value in (("filename", str(bag_source)), ("destination_path", ROOM_DIR),
                       ("destination_name", "SM_LFF_SensoryBeanBag"), ("automated", True),
                       ("replace_existing", True), ("save", True)):
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    meshes = [require_asset(path) for path in task.imported_object_paths
              if EAL.find_asset_data(path).asset_class_path.asset_name == "StaticMesh"]
    if len(meshes) != 1:
        raise RuntimeError(f"Expected one imported bean-bag mesh, found {task.imported_object_paths}")
    bag_mesh = meshes[0]
    textures = {name: import_texture(ROOT / "Saved/SensoryRoomImport" / f"T_LFF_Sensory_{name}.png",
                                     ROOM_DIR, f"T_LFF_Sensory_{name}")
                for name in ("Field", "Eagle", "Touch", "Carpet", "Fabric", "Bubbles")}
    mats = {
        "Wall": surface("Wall", (0.009, 0.038, 0.025)),
        "White": surface("White", (0.72, 0.73, 0.70)),
        "Trim": surface("Trim", (0.018, 0.019, 0.016)),
        "Metal": surface("Metal", (0.32, 0.34, 0.33), roughness=0.4),
        "Carpet": surface("Carpet", (1, 1, 1), textures["Carpet"]),
        "Fabric": surface("Fabric", (0.45, 0.45, 0.45), textures["Fabric"]),
        "Field": surface("Field", (1, 1, 1), textures["Field"]),
        "Eagle": surface("Eagle", (1, 1, 1), textures["Eagle"]),
        "Touch": surface("Touch", (1, 1, 1), textures["Touch"], glow=0.25),
        "Bubbles": surface("Bubbles", (1, 1, 1), textures["Bubbles"], glow=2.0),
        "Lamp": surface("Lamp", (0.85, 0.80, 0.65), glow=1.5),
    }
    for actor in subsystem.get_all_level_actors():
        if TAG in [str(tag) for tag in actor.tags]:
            if actor.get_attach_parent_actor() != room:
                raise RuntimeError(f"Refusing to remove refinement outside the sensory room: {actor.get_actor_label()}")
            if not subsystem.destroy_actor(actor):
                raise RuntimeError(f"Could not remove {actor.get_actor_label()}")
    for component in room.get_components_by_class(unreal.StaticMeshComponent):
        name = component.get_name()
        if name.startswith(("Panel",)):
            component.set_visibility(False)
            component.set_hidden_in_game(True)
        elif name.startswith("BeanBag"):
            component.set_static_mesh(bag_mesh)
            position = component.get_editor_property("relative_location")
            component.set_relative_location(unreal.Vector(position.x, position.y, 7.5), False, False)
            component.set_relative_scale3d(unreal.Vector(1, 1, 1))
            component.set_material(0, mats["Fabric"])
        else:
            key = "Carpet" if name == "Floor" else "White" if name == "Roof" else "Wall"
            component.set_material(0, mats[key])

    cube = require_asset("/Engine/BasicShapes/Cube")
    plane = require_asset("/Engine/BasicShapes/Plane")
    serial = 0

    def part(name, at, size, mat, mesh=cube, rotation=(0, 0, 0)):
        nonlocal serial
        serial += 1
        actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, room.get_actor_location())
        if actor is None:
            raise RuntimeError(f"Could not spawn {name}")
        actor.set_actor_label(f"SR_{name}_{serial:03d}")
        actor.set_editor_property("tags", [unreal.Name(TAG)])
        actor.set_folder_path("LFF/Props/SensoryRoom")
        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(mesh)
        component.set_material(0, mats[mat])
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        attach_room_actor(actor, room)
        actor.set_actor_relative_location(unreal.Vector(*at), False, False)
        actor.set_actor_relative_rotation(unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), False, False)
        actor.set_actor_relative_scale3d(unreal.Vector(*(v / 100 for v in size)))
        return actor

    # Planes map each photo once; cubes retain depth for frames, bases and furniture.
    def panel(name, at, width, height, mat, side=False):
        if side:
            part(name + "Frame", (at[0]-4, at[1], at[2]), (5, width+8, height+8), "Trim")
            part(name, at, (width, height, 100), mat, plane, (0, -90, 90))
        else:
            part(name + "Frame", (at[0], at[1]-4, at[2]), (width+8, 5, height+8), "Trim")
            part(name, at, (width, height, 100), mat, plane, (0, 0, 90))

    panel("FieldWall", (-318, -70, 157), 320, 160, "Field", side=True)
    panel("EagleArt", (70, -398, 195), 155, 110, "Eagle")
    for x in (-232, -150, 225):
        panel("BubblePanel", (x, -398, 188), 42, 145, "Bubbles")
        part("BubbleBase", (x, -392, 103), (46, 13, 28), "Metal")
        part("BubbleCap", (x, -392, 262), (46, 13, 8), "Metal")
    for y in (-285, 155):
        panel("TouchPanel", (-318, y, 157), 42, 85, "Touch", side=True)
    for x in (-315, 315):
        part("Skirting", (x, 0, 15), (6, 795, 18), "Trim")
    part("SkirtingBack", (0, -397, 15), (630, 6, 18), "Trim")
    for x in (-240, -80, 80, 240):
        for y in range(-350, 351, 100):
            part("CeilingTile", (x, y, 378), (157, 97, 3), "White")
    for x, y in ((-180, -170), (160, 110)):
        part("CeilingLamp", (x, y, 375), (48, 48, 3), "Lamp")
        light = subsystem.spawn_actor_from_class(unreal.RectLight, room.get_actor_location())
        if light is None:
            raise RuntimeError("Could not create room light")
        light.set_actor_label(f"SR_CeilingLight_{x}")
        light.set_editor_property("tags", [unreal.Name(TAG)])
        attach_room_actor(light, room)
        light.set_actor_relative_location(unreal.Vector(x, y, 370), False, False)
        light.set_actor_relative_rotation(unreal.Rotator(pitch=-90, yaw=0, roll=0), False, False)
        component = light.light_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        component.set_editor_property("intensity", 450.0)
        component.set_editor_property("source_width", 48.0)
        component.set_editor_property("source_height", 48.0)
        component.set_editor_property("attenuation_radius", 950.0)
        component.set_editor_property("cast_shadows", False)
        component.set_light_color(unreal.LinearColor(1, 0.94, 0.84))
    for x, y in ((-205, 65), (185, -265)):
        part("LightBench", (x, y, 34), (60, 135, 53), "White")
        part("BenchInset", (x, y, 61), (44, 110, 1), "Touch")
    for x, y in ((-270, -270), (-270, 135)):
        for i, mat in enumerate(("Touch", "Bubbles", "White")):
            part("ToyStorage", (x, y, 23+i*25), (38, 42, 24), mat)
    if not EAL.save_directory(ROOM_DIR):
        raise RuntimeError("Could not save all sensory-room assets")
    verify_room_attachments(room)
    unreal.log(f"[LFF sensory] Refined room with {serial} static mesh parts and two steady ceiling lights")


def main():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("Stop Play in Editor before refining sensory spaces")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name() != "/Game/LFF/Levels/L_LFF_Stadium.L_LFF_Stadium":
        raise RuntimeError("Open L_LFF_Stadium before running the sensory refinement")
    apply_trailer_wrap()
    refine_room()
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Could not save the refined stadium level")
    unreal.log("[LFF sensory] DONE; room placement and tour camera unchanged")


if __name__ == "__main__":
    main()
