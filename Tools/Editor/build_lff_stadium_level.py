"""Builds /Game/LFF/Levels/L_LFF_Stadium and imports DT_LFF_GoldenPath (Docs/ManualSetup.md §2-§4).

Run headless with the editor closed (see Docs/LOCAL_AGENT_SETUP.md §3):
  UnrealEditor VirtualStadium.uproject -ExecutePythonScript="<abs path to this file>" -ModelContextProtocolPort=18000 -unattended -nullrhi -nosplash -stdout
(The port override keeps this run's MCP server, and the crash reporter that inherits its socket, off 8000.)

Refuses to overwrite an existing level, so it can't clobber hand-framed shots. Delete the map first to rebuild.

Poses are in the local stadium frame: origin = midfield turf, +X across the field (toward the east stand),
+Y along the field (toward the south end), +Z up, cm. LFF_SiteRoot is then rotated by SITE_HEADING_DEG so
this frame lines up with the real field. The origin, heading and the gate/seat poses were calibrated against
the Google tiles on 2026-09-28 (Docs/CESIUM_PLACEMENT.md, revision 2026-09-28a). Fine framing is still
done by piloting each CT_ target (ManualSetup §3).
"""
import os
import unreal

LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
TABLE_DIR, TABLE_NAME = "/Game/LFF/Data", "DT_LFF_GoldenPath"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
JOURNEY_JSON = os.path.join(PROJECT_DIR, "SourceData", "Journey", "LFF_GoldenPath.json")

# Placement revision 2026-09-28a (CESIUM_PLACEMENT.md): midfield from overhead captures and pitch-wall
# traces (about ±1 m), turf height from line traces onto the tiles (ellipsoidal, about ±0.3 m).
ORIGIN_LAT, ORIGIN_LON, ORIGIN_HEIGHT_ELLIPSOID_M = 39.9008036, -75.1674790, -31.04
SITE_HEADING_DEG = -6.5  # field axis about 353.5 deg true (pitch-wall traces and line tilt, ±1 deg)
GOOGLE_PHOTOREALISTIC_ION_ASSET = 2275207
PHILADELPHIA_UTC_OFFSET_OCT = -4.0  # EDT

CUBE = "/Engine/BasicShapes/Cube.Cube"  # 100 cm cube, pivot at centre
SIGN_PLATE_MATERIAL = "/Game/LFF/Materials/MI_LFF_SignPlate"
# Dark midnight green (#003338-ish, linear). Text and plate are both lit, so their contrast comes from the
# albedos (white about 0.9 against about 0.04, roughly 10:1) and holds in sun and shade (ACCESSIBILITY.md: 7:1).
SIGN_PLATE_COLOR = (0.0, 0.035, 0.045)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def log(msg):
    unreal.log(f"[LFF build] {msg}")


def spawn(cls, label, loc=(0, 0, 0), rot=(0, 0, 0), parent=None, folder=None):
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    actor.set_actor_label(label)
    if parent:
        actor.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD, False)
    if folder:
        actor.set_folder_path(folder)
    return actor


def add_component(actor, cls):
    handles = sds.k2_gather_subobject_data_for_instance(actor)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=None)
    handle, fail = sds.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f"add {cls} to {actor.get_actor_label()}: {fail}")
    return actor.get_components_by_class(cls)[-1]


def sign_plate_material():
    if unreal.EditorAssetLibrary.does_asset_exist(SIGN_PLATE_MATERIAL):
        return unreal.load_asset(SIGN_PLATE_MATERIAL)
    folder, name = SIGN_PLATE_MATERIAL.rsplit("/", 1)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*SIGN_PLATE_COLOR, 1.0))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.9)  # no glare
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def look_at(frm, to):
    r = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*frm), unreal.Vector(*to))
    return (r.pitch, r.yaw, 0.0)


def box(label, center, size_cm, parent, folder):
    a = spawn(unreal.StaticMeshActor, label, center, folder=folder)
    smc = a.static_mesh_component
    # Static components can't attach under the movable, globe-anchored LFF_SiteRoot. Lighting is all dynamic.
    smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(unreal.load_asset(CUBE))
    a.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                      unreal.AttachmentRule.KEEP_WORLD, False)
    a.set_actor_scale3d(unreal.Vector(size_cm[0] / 100.0, size_cm[1] / 100.0, size_cm[2] / 100.0))
    return a


def text(label, value, loc, face_toward, world_size, parent, folder, tags=()):
    """A sign: white text on a dark plate. The plate is a component of the label actor, so the C++
    SetActorHiddenInGame on LFF_Label actors shows and hides both together."""
    # A TextRender reads correctly from its +X side, so point +X back at the viewer.
    p, y, _ = look_at(loc, face_toward)
    a = spawn(unreal.TextRenderActor, label, loc, (0.0, y, 0.0), parent=parent, folder=folder)
    trc = a.text_render
    trc.set_editor_property("text", value)
    trc.set_editor_property("world_size", world_size)
    trc.set_editor_property("text_render_color", unreal.Color(r=255, g=255, b=255, a=255))
    trc.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    trc.set_editor_property("vertical_alignment", unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    trc.set_editor_property("cast_shadow", False)  # keep the text's own shadow off the plate
    a.tags = [unreal.Name(t) for t in tags]

    size = trc.get_text_local_size()
    width = max(size.x, size.y) if max(size.x, size.y) > 1.0 else 0.55 * world_size * len(value)
    height = size.z if size.z > 1.0 else world_size
    margin = 0.35 * world_size
    plate = add_component(a, unreal.StaticMeshComponent)
    plate.set_static_mesh(unreal.load_asset(CUBE))
    plate.set_material(0, sign_plate_material())
    plate.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    # Just behind the text (text faces +X); 2 cm thick.
    plate.set_relative_location(unreal.Vector(-2.0, 0.0, 0.0), False, False)
    plate.set_relative_scale3d(unreal.Vector(0.02, (width + 2 * margin) / 100.0, (height + 2 * margin) / 100.0))
    return a


def target(target_id, loc, look, parent):
    a = spawn(unreal.LFFCameraTarget, target_id, loc, look_at(loc, look), parent=parent, folder="LFF_SiteRoot/LFF_Targets")
    a.set_editor_property("target_id", target_id)
    return a


def build_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        raise RuntimeError(f"{LEVEL} already exists; delete it first to rebuild")
    if not les.new_level(LEVEL, False):
        raise RuntimeError("new_level failed")

    geo = spawn(unreal.CesiumGeoreference, "CesiumGeoreference", folder="Cesium")
    # Without this tag Cesium spawns its own default georeference (at its Denver demo origin) and
    # everything that resolves the default ends up there instead of here.
    geo.tags = [unreal.Name("DEFAULT_GEOREFERENCE")]
    geo.set_editor_property("origin_placement", unreal.OriginPlacement.CARTOGRAPHIC_ORIGIN)
    geo.set_editor_property("origin_latitude", ORIGIN_LAT)
    geo.set_editor_property("origin_longitude", ORIGIN_LON)
    geo.set_editor_property("origin_height", ORIGIN_HEIGHT_ELLIPSOID_M)

    tiles = spawn(unreal.Cesium3DTileset, "Tileset_Context", folder="Cesium")
    tiles.set_editor_property("tileset_source", unreal.TilesetSource.FROM_CESIUM_ION)
    tiles.set_editor_property("ion_asset_id", GOOGLE_PHOTOREALISTIC_ION_ASSET)
    tiles.set_editor_property("georeference", geo)
    # No token here: the tileset uses the project default from CesiumIonSaaS (set via the Cesium panel).

    sun = spawn(unreal.CesiumSunSky, "CesiumSunSky", folder="Cesium")
    for prop, value in (("time_zone", PHILADELPHIA_UTC_OFFSET_OCT), ("solar_time", 13.0), ("month", 10), ("day", 9)):
        try:
            sun.set_editor_property(prop, value)
        except Exception as e:  # property names are Cesium-version dependent
            log(f"CesiumSunSky.{prop} not set: {e}")

    root = spawn(unreal.Actor, "LFF_SiteRoot")
    add_component(root, unreal.SceneComponent)
    add_component(root, unreal.CesiumGlobeAnchorComponent)
    root.get_component_by_class(unreal.CesiumGlobeAnchorComponent).set_editor_property("georeference", geo)
    root.tags = [unreal.Name("LFF_SiteRoot")]

    # Step 1: exterior arrival, north plaza (Illustrative).
    ext_cam, ext_look = (-6000, -25000, 1800), (0, 0, 1500)
    target("CT_ExteriorArrival", ext_cam, ext_look, root)
    text("LBL_Plaza", "Plaza (illustration)", (-4500, -19500, 900), ext_cam, 350, root, "LFF_SiteRoot/LFF_Labels",
         ("LFF_Label", "LBL_Plaza"))

    # Step 2: gate entry, 35 m outside the north headhouse facade (about 135 m north of midfield; plaza
    # about +0.7 m), standing eye height (Illustrative; not a real gate name).
    gate_cam, gate_look = (-1500, -17000, 260), (0, -13500, 600)
    target("CT_GateEntry", gate_cam, gate_look, root)
    text("LBL_Gate", "Gate (illustration)", (0, -14200, 600), gate_cam, 200, root, "LFF_SiteRoot/LFF_Labels",
         ("LFF_Label", "LBL_Gate"))

    # Step 3: the model's open-air perimeter concourse at plaza level on the west side (ADR-0004). Probed in the
    # model: floor +1.58 m, ~6 m clear height, inner wall at u ~ -72.3 m, open to the west, long run north-south.
    # Standing eye height, looking north along the concourse; signs on the inner wall (on the visitor's right).
    lbl = "LFF_SiteRoot/LFF_Labels"
    floor_z = 158
    conc_cam, conc_look = (-7800, 800, floor_z + 165), (-7800, -3000, floor_z + 150)
    target("CT_InteriorConcourse", conc_cam, conc_look, root)
    text("LBL_Concourse", "Concourse (illustration)", (-7800, -200, floor_z + 390), conc_cam, 60, root, lbl,
         ("LFF_Label", "LBL_Concourse"))
    text("LBL_Restroom", "Restrooms (example)", (-7240, -400, floor_z + 230), (-8000, -400, floor_z + 230),
         50, root, lbl, ("LFF_Label", "LBL_Restroom"))
    text("LBL_Food", "Food (example)", (-7262, -1200, floor_z + 230), (-8000, -1200, floor_z + 230),
         50, root, lbl, ("LFF_Label", "LBL_Food"))

    # Step 4: example seat, about row 10 of the west lower bowl on the halfway line, looking at midfield.
    # Traced: pitch wall at 41.5 m, seats rise 0.25 m per metre, so the seat is about 3.9 m up; eye +1.3 m.
    # Framed against the tiles first; checked against the CGTrader model (its 3D seats fill the foreground).
    seat_cam, seat_look = (-5050, 0, 520), (0, 0, 0)
    target("CT_SeatFieldView", seat_cam, seat_look, root)
    # 3 m ahead and about 0.9 m below eye level, so the sign sits in the lower third and doesn't cover the pitch
    # (seat ramp there is about 3.1 m, so the sign clears it). "Example ticket" covers the illustration notice.
    text("LBL_Section", "Section 111, Row 10 (example ticket)", (-4750, 0, 430), seat_cam, 12, root, lbl,
         ("LFF_Label", "LBL_Section"))

    # Align the whole site frame with the real field. Children follow; the anchor updates to the new pose.
    root.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=SITE_HEADING_DEG), False)

    validate()
    if not les.save_current_level():
        raise RuntimeError("save_current_level failed")
    log(f"saved {LEVEL}")


def validate():
    """Structural checks from CESIUM_PLACEMENT §6 V9/V10 that can run without tiles."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    georefs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CesiumGeoreference)
    if len(georefs) != 1:
        raise RuntimeError(f"expected 1 CesiumGeoreference, found {[g.get_actor_label() for g in georefs]}")
    # CesiumSunSky carries its own built-in anchor; ADR-0001's single-anchor rule covers the site hierarchy.
    anchors = [a for a in eas.get_all_level_actors()
               if a.get_component_by_class(unreal.CesiumGlobeAnchorComponent) and not isinstance(a, unreal.CesiumSunSky)]
    if [a.get_actor_label() for a in anchors] != ["LFF_SiteRoot"]:
        raise RuntimeError(f"expected only LFF_SiteRoot to be globe anchored, found {[a.get_actor_label() for a in anchors]}")
    if any(a.get_component_by_class(unreal.CesiumOriginShiftComponent) for a in eas.get_all_level_actors()):
        raise RuntimeError("CesiumOriginShiftComponent found (ADR-0001 forbids it)")
    ids = sorted(str(t.get_editor_property("target_id")) for t in eas.get_all_level_actors() if isinstance(t, unreal.LFFCameraTarget))
    if ids != ["CT_ExteriorArrival", "CT_GateEntry", "CT_InteriorConcourse", "CT_SeatFieldView"]:
        raise RuntimeError(f"camera targets: {ids}")
    log(f"validate OK: 1 georeference, 1 anchor (LFF_SiteRoot), 0 origin shift, targets {ids}")


def import_table():
    row_struct = unreal.load_object(None, "/Script/VirtualStadium.LFFJourneyStep")
    # In UE 5.8 only the reimport subclass registers the .json format; it imports new tables too.
    factory = unreal.ReimportDataTableFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
    settings.set_editor_property("import_row_struct", row_struct)
    task = unreal.AssetImportTask()
    for prop, value in (("filename", JOURNEY_JSON), ("destination_path", TABLE_DIR), ("destination_name", TABLE_NAME),
                        ("automated", True), ("save", True), ("replace_existing", True), ("factory", factory)):
        task.set_editor_property(prop, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    table = unreal.load_asset(f"{TABLE_DIR}/{TABLE_NAME}")
    if not table:
        raise RuntimeError("DT_LFF_GoldenPath import failed")
    rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    log(f"imported {TABLE_NAME}: {rows}")


try:
    import_table()
    build_level()
    log("DONE")
except Exception as e:
    unreal.log_error(f"[LFF build] FAILED: {e}")
finally:
    unreal.SystemLibrary.quit_editor()
