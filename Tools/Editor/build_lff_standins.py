"""Places the journey's stand-in props, their labels and the extra camera targets in L_LFF_Stadium through Unreal MCP.

Run from the repo root with the editor open on L_LFF_Stadium and the MCP server running:
  python3 Tools/Editor/build_lff_standins.py

Safe to rerun: it first removes everything it placed before (actors tagged LFF_StandIn), then rebuilds and saves
the level. It never touches the Cesium actors, LFF_SiteRoot itself, the stadium model or the four original CT_
targets (it only moves LBL_Plaza up out of the caption's way).

Everything is Illustrative (LEDGER D9): simple, cheap shapes so a step isn't an empty plaza. No photoreal interiors.
Session 3 adds the moving pieces (ALFFCueActor: three gate proxies, the arriving car, the eagle's pass, soft
fireworks), the popcorn cart built to the brief, 10 parked cars, and a low crowd of 15 plain
stand-in people (tag LFF_Crowd; no jerseys, nobody in the bowl). The cars are City Sample Vehicles (seven car models;
no trucks, vans or buses); people are still primitives until the crowd pass (Docs/KNOWN_LIMITATIONS.md). Poses are in the local stadium frame (Tools/Editor/build_lff_stadium_level.py): +X across the field
toward the east stand, +Y along it toward the south end, cm. Heights are traced onto the model, so props sit on it.
Labels sit above their prop, in the upper half of the step's framing (the journey also hides any label that would
land in the lower third of the frame, where the caption card is).

After a rerun, run Tools/Editor/finish_pass.py: it re-aims CT_Parking, hides the plaza people and the editor icons, and
fills the rest of the lot and the lower bowl (session 4).
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from unreal_mcp import call  # noqa: E402

SC = "editor_toolset.toolsets.scene.SceneTools"
AT = "editor_toolset.toolsets.actor.ActorTools"
PT = "editor_toolset.toolsets.primitive.PrimitiveTools"
OT = "editor_toolset.toolsets.object.ObjectTools"
MI = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"
MT = "editor_toolset.toolsets.material.MaterialTools"
TT = "editor_toolset.toolsets.texture.TextureTools"
AS = "editor_toolset.toolsets.asset.AssetTools"

LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
MAT_DIR = "/Game/LFF/Materials"
SIGN_PLATE = f"{MAT_DIR}/MI_LFF_SignPlate.MI_LFF_SignPlate"
TAG = "LFF_StandIn"
FOLDER_PROPS = "LFF_SiteRoot/LFF_StandIns"
FOLDER_LABELS = "LFF_SiteRoot/LFF_Labels"
FOLDER_TARGETS = "LFF_SiteRoot/LFF_Targets"
FOLDER_CROWD = "LFF_SiteRoot/LFF_Crowd"
SITE_HEADING_DEG = -6.5  # LFF_SiteRoot yaw; only used to turn local poses into world points for traces
EYE = 165  # standing eye height above the traced ground

# Linear colours on /Engine/BasicShapes/BasicShapeMaterial (parameter "Color"). Rough, so nothing glares.
COLOURS = {
    "Bone": (0.80, 0.77, 0.70), "Midnight": (0.0, 0.07, 0.085), "Red": (0.55, 0.03, 0.03), "Grey": (0.22, 0.22, 0.23),
    "Dark": (0.015, 0.015, 0.018), "Blue": (0.04, 0.16, 0.50), "Silver": (0.45, 0.46, 0.48), "Paint": (0.85, 0.85, 0.82),
    "Mannequin": (0.50, 0.49, 0.47),
}


def world_xy(x, y):
    c, s = math.cos(math.radians(SITE_HEADING_DEG)), math.sin(math.radians(SITE_HEADING_DEG))
    return x * c - y * s, x * s + y * c


_GROUND = {}


def ground(x, y, above=350):
    """Traced height of the model (or tiles) under a local point. Starts `above` cm over plaza level, under the
    concourse deck (about 6 m clear), so it finds the floor it's on. Cached per metre, so a label or a camera
    aimed at a prop reads the bare ground under it (traced before the prop was placed), not the prop's roof."""
    key = (round(x / 100), round(y / 100))
    if key not in _GROUND:
        _GROUND[key] = _trace_ground(x, y, above)
    return _GROUND[key]


def _trace_ground(x, y, above):
    wx, wy = world_xy(x, y)
    start = GROUND_GUESS + above
    dist = call(SC, "trace_world", {"start": {"x": wx, "y": wy, "z": start}, "end": {"x": wx, "y": wy, "z": start - 3000}})
    if dist is None:
        raise RuntimeError(f"no ground under local ({x}, {y})")
    return start - dist


GROUND_GUESS = 158  # plaza and plaza-level concourse, both about +1.6 m (build_lff_stadium_level.py)


def xform(loc, rot=(0, 0, 0), scale=(1, 1, 1)):
    return {"location": dict(zip("xyz", loc)), "rotation": {"pitch": rot[0], "yaw": rot[1], "roll": rot[2]},
            "scale": dict(zip("xyz", scale))}


def look(frm, to):
    dx, dy, dz = (to[i] - frm[i] for i in range(3))
    return math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx))


def set_props(obj, values):
    if not call(OT, "set_properties", {"instance": obj, "values": json.dumps(values)}):
        raise RuntimeError(f"set_properties failed on {obj['refPath']}: {list(values)}")


def material(name):
    path = f"{MAT_DIR}/MI_LFF_StandIn_{name}"
    ref = {"refPath": f"{path}.MI_LFF_StandIn_{name}"}
    if not call(AS, "exists", {"path": path}):
        call(MI, "create", {"folder_path": MAT_DIR, "asset_name": f"MI_LFF_StandIn_{name}",
                            "parent": {"refPath": "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"}})
    r, g, b = COLOURS[name]
    call(MI, "set_vector_parameter", {"instance": ref, "name": "Color", "value": {"r": r, "g": g, "b": b, "a": 1}})
    call(MI, "set_scalar_parameter", {"instance": ref, "name": "Roughness", "value": 0.85})
    call(AS, "save_assets", {"asset_paths": [path]})
    return ref


class Prop:
    """An actor under LFF_SiteRoot built from cubes, cylinders and spheres in its own frame (+X faces the viewer).
    `cls` may be an ALFFCueActor class: then the parts are Movable so the whole prop can slide or roll."""

    def __init__(self, label, x, y, face=None, yaw=0.0, folder=FOLDER_PROPS, z=None, cls="/Script/Engine.Actor",
                 tags=()):
        self.x, self.y = x, y
        self.z = ground(x, y) if z is None else z
        if face is not None:
            yaw = look((x, y, 0), (face[0], face[1], 0))[1]
        self.yaw = yaw
        self.movable = cls != "/Script/Engine.Actor"
        self.actor = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": cls}, "name": label,
                                                         "xform": xform((x, y, self.z), (0, yaw, 0)), "parent": SITE_ROOT,
                                                         "snap_to_ground": False})
        call(AT, "set_label", {"actor": self.actor, "label": label})
        for tag in (TAG, *tags):
            call(AT, "add_tag", {"actor": self.actor, "tag": tag})
        call(SC, "set_actor_folder", {"actor": self.actor, "folder_path": folder})
        self.n = 0

    def local(self, ox, oy, oz=0.0):
        """A point in this prop's frame, in site-local coordinates."""
        c, s_ = math.cos(math.radians(self.yaw)), math.sin(math.radians(self.yaw))
        return self.x + ox * c - oy * s_, self.y + ox * s_ + oy * c, self.z + oz

    def _finish(self, comp, mat):
        values = {"OverrideMaterials": [MATS[mat]]}
        if self.movable:
            values["Mobility"] = "Movable"
        set_props(comp, values)
        return comp

    def box(self, size, at, mat, rot=(0, 0, 0)):
        self.n += 1
        c = call(PT, "add_cube", {"actor": self.actor, "name": f"Box{self.n}", "dimensions": dict(zip("xyz", size)),
                                  "local_transform": xform(at, rot)})
        return self._finish(c, mat)

    def cyl(self, radius, height, at, mat, rot=(0, 0, 0)):
        self.n += 1
        c = call(PT, "add_cylinder", {"actor": self.actor, "name": f"Cyl{self.n}", "radius": radius, "height": height,
                                      "local_transform": xform(at, rot)})
        return self._finish(c, mat)

    def ball(self, radius, at, mat):
        self.n += 1
        c = call(PT, "add_sphere", {"actor": self.actor, "name": f"Ball{self.n}", "radius": radius, "local_transform": xform(at)})
        return self._finish(c, mat)

    def mesh(self, path, overrides=None):
        """A static mesh part at the prop's origin (the City Sample car parts are modelled in place)."""
        self.n += 1
        c = call(AT, "add_component", {"owner": self.actor, "component_type": {"refPath": "/Script/Engine.StaticMeshComponent"},
                                       "name": f"Mesh{self.n}"})
        values = {"StaticMesh": {"refPath": path}}
        if overrides:
            values["OverrideMaterials"] = overrides
        if self.movable:
            values["Mobility"] = "Movable"
        set_props(c, values)
        return c

    def model(self, key, at, yaw=0.0, overrides=None):
        """One of the imported prop models (MODELS), scaled, turned by `yaw` and placed so its footprint's centre
        sits at `at` with its lowest point on at's z. `overrides` maps slot index -> material ref."""
        path, scale, (cx, cy, zmin) = MODELS[key]
        c_, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        ox, oy = scale * (cx * c_ - cy * s_), scale * (cx * s_ + cy * c_)
        slots = None
        if overrides:
            slots = [None] * (max(overrides) + 1)
            for i, mat in overrides.items():
                slots[i] = mat
        comp = self.mesh(path, slots)
        set_props(comp, {"RelativeLocation": dict(zip("xyz", (at[0] - ox, at[1] - oy, at[2] - scale * zmin))),
                         "RelativeRotation": {"pitch": 0, "yaw": yaw, "roll": 0},
                         "RelativeScale3D": {"x": scale, "y": scale, "z": scale}})
        return comp

    def logo(self, mi, at, width, height):
        """A logo (a masked MI) on the engine's 1 m plane, facing +X. The plane maps its texture once; the cubes
        from add_cube map one repeat per metre, which crops a small panel. Yaw -90 / roll 90 turns the plane's face
        to +X with the texture's width running to the viewer's right (-Y) and its top up."""
        c = self.mesh("/Engine/BasicShapes/Plane.Plane", [{"refPath": mi}])
        set_props(c, {"RelativeLocation": dict(zip("xyz", at)), "RelativeRotation": {"pitch": 0, "yaw": -90, "roll": 90},
                      "RelativeScale3D": {"x": width / 100, "y": height / 100, "z": 1}, "CastShadow": False})
        return c

    def cue(self, cue_id, start_offset=(0, 0, 0), seconds=3.0, delay=0.0, trigger="Arrival", linear=False,
            hidden_before=False, hidden_after=False):
        set_props(self.actor, {"CueId": cue_id, "Trigger": trigger, "PlaySeconds": seconds, "PlayDelaySeconds": delay,
                               "bLinear": linear, "bHiddenBefore": hidden_before, "bHiddenAfter": hidden_after,
                               "StartOffset": dict(zip("xyz", start_offset))})


def painted_text(prop, text, at, size, colour, name):
    """Lettering painted on a prop face (always visible, not a journey label): a TextRenderActor facing the prop's +X."""
    x, y, z = prop.local(*at)
    a = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": "/Script/Engine.TextRenderActor"}, "name": name,
                                            "xform": xform((x, y, z), (0, prop.yaw, 0)), "parent": SITE_ROOT, "snap_to_ground": False})
    call(AT, "set_label", {"actor": a, "label": name})
    call(AT, "add_tag", {"actor": a, "tag": TAG})
    call(SC, "set_actor_folder", {"actor": a, "folder_path": FOLDER_PROPS})
    trc = [c for c in call(AT, "get_components", {"actor": a, "component_type": None}) if "TextRender" in c["refPath"]][0]
    set_props(trc, {"Text": text, "WorldSize": size, "TextRenderColor": dict(zip("rgba", (*(v / 255 for v in colour), 1))),  # 0-1 over MCP
                    "HorizontalAlignment": "EHTA_Center", "VerticalAlignment": "EVRTA_TextCenter", "CastShadow": False})
    return a


def sign(label_id, text, x, y, height, face, world_size, lines=1):
    """A world label: white text on a dark plate (same look as build_lff_stadium_level.py). Hidden unless a step
    lists it in LabelsToShow."""
    z = ground(x, y) + height
    yaw = look((x, y, z), face)[1]
    a = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": "/Script/Engine.TextRenderActor"}, "name": label_id,
                                            "xform": xform((x, y, z), (0, yaw, 0)), "parent": SITE_ROOT, "snap_to_ground": False})
    call(AT, "set_label", {"actor": a, "label": label_id})
    for t in ("LFF_Label", label_id, TAG):
        call(AT, "add_tag", {"actor": a, "tag": t})
    call(SC, "set_actor_folder", {"actor": a, "folder_path": FOLDER_LABELS})
    trc = [c for c in call(AT, "get_components", {"actor": a, "component_type": None}) if "TextRender" in c["refPath"]][0]
    set_props(trc, {"Text": text, "WorldSize": world_size, "TextRenderColor": {"r": 255, "g": 255, "b": 255, "a": 255},
                    "HorizontalAlignment": "EHTA_Center", "VerticalAlignment": "EVRTA_TextCenter", "CastShadow": False,
                    "Mobility": "Movable"})
    longest = max(len(part) for part in text.split("<br>"))
    width, tall, margin = 0.52 * world_size * longest, world_size * lines * 1.1, 0.35 * world_size
    plate = call(PT, "add_cube", {"actor": a, "name": "Plate", "dimensions": {"x": 2, "y": width + 2 * margin, "z": tall + 2 * margin},
                                  "local_transform": xform((-2, 0, 0))})
    set_props(plate, {"OverrideMaterials": [{"refPath": SIGN_PLATE}], "CastShadow": False})
    try:
        set_props(plate, {"BodyInstance": {"CollisionProfileName": "NoCollision"}})  # signs never block the camera check
    except Exception as e:  # noqa: BLE001 - property shape can differ between engine versions
        print(f"  {label_id}: plate collision left at default ({e})")
    return a


def target(target_id, cam_xy, look_at, eye=EYE, fov=70.0, look_z_is_absolute=False, cam_z=None):
    x, y = cam_xy
    z = ground(x, y) + eye if cam_z is None else cam_z
    lz = look_at[2] if look_z_is_absolute else ground(look_at[0], look_at[1]) + look_at[2]
    pitch, yaw = look((x, y, z), (look_at[0], look_at[1], lz))
    a = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": "/Script/VirtualStadium.LFFCameraTarget"},
                                            "name": target_id, "xform": xform((x, y, z), (pitch, yaw, 0)),
                                            "parent": SITE_ROOT, "snap_to_ground": False})
    call(AT, "set_label", {"actor": a, "label": target_id})
    call(AT, "add_tag", {"actor": a, "tag": TAG})
    call(SC, "set_actor_folder", {"actor": a, "folder_path": FOLDER_TARGETS})
    set_props(a, {"TargetId": target_id, "FieldOfView": fov})
    return (x, y, z)


CUE = "/Script/VirtualStadium.LFFCueActor"
FIREWORKS = "/Script/VirtualStadium.LFFFireworksActor"
GEOMETRY_CACHE_ACTOR = "/Script/GeometryCache.GeometryCacheActor"
EAGLE_CACHE = "/Game/LFF/Eagle/GC_LFF_BaldEagle.GC_LFF_BaldEagle"
GLOW = "/Game/LFF/Materials/MI_LFF_FireworkGlow.MI_LFF_FireworkGlow"
# Logos from Assets/Logos only (Tools/Pipeline/prepare_logos.py, imported by import_lff_content.py step "logos").
LOGO_EAF = "/Game/LFF/Logos/MI_LFF_Logo_EAF.MI_LFF_Logo_EAF"
LOGO_WAWA = "/Game/LFF/Logos/MI_LFF_Logo_Wawa.MI_LFF_Logo_Wawa"
LOGO_MIDFIELD = "/Game/LFF/Logos/MI_LFF_LogoDecal_Eagles.MI_LFF_LogoDecal_Eagles"
# Lettering colour (FColor, sRGB): near-black.
INK = (20, 20, 22)

# Prop models from Assets/*.gltf (Tools/Editor/import_lff_props.py): mesh, uniform scale to real size, and the
# mesh's own footprint centre (x, y) and lowest z, in its units (the files came in at different scales and pivots).
# Each faces the viewer after the yaw its function gives it.
PROPS = "/Game/LFF/Props"
MODELS = {
    # 32 x 21 x 27 cm, its wide face toward -Y. Slot 1 is the bag's plastic (alpha 0 in the file): M_LFF_ClearBag.
    "Bag": (f"{PROPS}/Bag/SM_LFF_Bag.SM_LFF_Bag", 1.0, (2, -10, 1)),
    # A bike-rack barrier, along X. At 0.103 a section is 2.75 m long and 1.1 m tall: four to an 11 m lane line.
    "Barrier": (f"{PROPS}/Barrier/SM_LFF_Barrier.SM_LFF_Barrier", 0.103, (-6, 0, -153)),
    # A walk-through metal detector, walked through along X. At 0.08: 2.3 m tall, 1.03 m wide.
    "MetalDetector": (f"{PROPS}/MetalDetector/SM_LFF_MetalDetector.SM_LFF_MetalDetector", 0.08, (1116, -1028, -61)),
    # A concession stand at real size: 15.2 m long, 3.6 m tall, its counters toward -Y.
    "Concession": (f"{PROPS}/Concession/SM_LFF_Concession.SM_LFF_Concession", 1.0, (45, -115, -8)),
    # The Sensory Activation Vehicle, tongue toward -Y, door side toward +X. Its side wrap is the real trailer's,
    # from the reference photo (Tools/Pipeline/prepare_sav_wrap.py). At 0.065 the body is 4.1 m long and 1.9 m tall
    # on 0.76 m wheels, as in the photo (5.4 m with the tongue).
    "SensoryVehicle": (f"{PROPS}/SensoryVehicle/SM_LFF_SensoryVehicle.SM_LFF_SensoryVehicle", 0.065, (484, -1660, -90)),
}


# City Sample Vehicles (Fab, /Game/CitySampleVehicles): the five everyday cars. Not the vans, trucks, garbage truck,
# trailer or bus, and not vehicle12 (a taxi, roof sign) or vehicle13 (a police car, light bar): a fan car park. Each car is its body, four wheels and glass as static meshes (all modelled in place, the
# body and wheels Nanite): no rig, no Chaos vehicle, nothing ticks. Front is +X, like the stand-in car.
CITY_CARS = ("02", "03", "05", "06", "07")
# Paint (linear BaseColor on a child of the car's own paint MI, its palette lookup switched off): the usual
# car-park mix, mostly neutrals.
CAR_PAINTS = {"White": (0.70, 0.70, 0.68), "Black": (0.012, 0.012, 0.014), "Silver": (0.36, 0.37, 0.38),
              "Grey": (0.12, 0.125, 0.13), "Navy": (0.012, 0.03, 0.09), "Red": (0.30, 0.015, 0.012),
              "Beige": (0.42, 0.36, 0.26), "Green": (0.01, 0.07, 0.06)}
SM_ = "editor_toolset.toolsets.static_mesh.StaticMeshTools"
_PAINT_MI = {}


def car_paint(model, colour):
    """(slot index of the body's paint, its override MI) for one car model in one colour; made once, saved."""
    key = (model, colour)
    if key not in _PAINT_MI:
        body = {"refPath": f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh/SM_vehCar_vehicle{model}_No_Wheel."
                           f"SM_vehCar_vehicle{model}_No_Wheel"}
        slots = call(SM_, "get_material_slots", {"mesh": body})
        index = slots.index("veh_carPaint")
        parent = call(SM_, "get_material", {"mesh": body, "slot_name": "veh_carPaint"})
        name = f"MI_LFF_CarPaint_{model}_{colour}"
        path = f"{MAT_DIR}/Cars/{name}"
        if not call(AS, "exists", {"path": path}):
            call(MI, "create", {"folder_path": f"{MAT_DIR}/Cars", "asset_name": name, "parent": parent})
        ref = {"refPath": f"{path}.{name}"}
        call(MI, "set_static_switch_parameter", {"instance": ref, "name": "Paint Variation", "value": False})
        r, g, b = CAR_PAINTS[colour]
        call(MI, "set_vector_parameter", {"instance": ref, "name": "BaseColor", "value": {"r": r, "g": g, "b": b, "a": 1}})
        call(AS, "save_assets", {"asset_paths": [path]})
        _PAINT_MI[key] = (index, len(slots), ref)
    return _PAINT_MI[key]


def city_car(label, x, y, model, colour, face_yaw, cls="/Script/Engine.Actor"):
    """A parked City Sample car (see CITY_CARS)."""
    c = Prop(label, x, y, yaw=face_yaw, cls=cls)
    base = f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh"
    index, count, paint = car_paint(model, colour)
    overrides = [None] * count
    overrides[index] = paint
    c.mesh(f"{base}/SM_vehCar_vehicle{model}_No_Wheel.SM_vehCar_vehicle{model}_No_Wheel", overrides)
    for wheel in ("Front_L", "Front_R", "Rear_L", "Rear_R"):
        c.mesh(f"{base}/SM_Wheel_{wheel}_vehCar_vehicle{model}.SM_Wheel_{wheel}_vehCar_vehicle{model}")
    c.mesh(f"{base}/SM_All_Trans_vehCar_vehicle{model}.SM_All_Trans_vehCar_vehicle{model}")
    return c


def person(label, x, y, yaw, shirt, pants, scale=1.0, crowd=True):
    """A stand-in visitor in plain colours (no jerseys). Crowd people are tagged LFF_Crowd, so Crowd Off and Calm
    Mode hide them; staff (crowd=False) always stay."""
    m = Prop(label, x, y, yaw=yaw, folder=FOLDER_CROWD if crowd else FOLDER_PROPS, tags=("LFF_Crowd",) if crowd else ())
    k = scale
    for ly in (-10, 10):
        m.cyl(8 * k, 84 * k, (0, ly * k, 42 * k), pants)
    m.cyl(19 * k, 62 * k, (0, 0, 115 * k), shirt)
    for ay in (-27, 27):
        m.cyl(5.5 * k, 58 * k, (0, ay * k, 110 * k), shirt)
    m.ball(11 * k, (0, 0, 160 * k), "Mannequin")
    return m


# People: MetaHuman Crowd Sample (engine plugin MetaHumanCrowdContent) bodies and faces, dressed in its cloth outfits
# (a long-sleeve tee, jeans, sneakers) on an ALFFPersonActor. Tees in Eagles colours with the Eagles head on the chest
# (Assets/Clothing Textures, as a print mask: Tools/Pipeline/prepare_people_prints.py); staff wear black "STAFF" tees.
MH = "/MetaHumanCrowdContent"
PERSON = "/Script/VirtualStadium.LFFPersonActor"
MH_PEOPLE = (("AverageMale_Head01", "m"), ("AverageFemale_Head01", "f"), ("AverageMale_Head02", "m"),
             ("AverageFemale_Head02", "f"), ("TallMale_Head01", "m"), ("ShortFemale_Head01", "f"))
MH_TOP = MH + "/Outfits/Tops/OA_TshirtLngSlv/TshirtLngSlv/ClothAssets/CA_TshirtLngSlv_{0}_med_nrw.CA_TshirtLngSlv_{0}_med_nrw"
MH_JEANS = MH + "/Outfits/Bottoms/OA_Jeans/Jeans/ClothAssets/CA_jeans_{0}_med_nrw.CA_jeans_{0}_med_nrw"
MH_SHOES = MH + "/Outfits/Shoes/OA_CasualSneakers/CasualSneakers/ClothAssets/CA_casualsneakers_{0}_med_nrw.CA_casualsneakers_{0}_med_nrw"
# The tee's own material (its crowd instance hides the print graphic switch).
MH_TEE_PARENT = MH + "/Outfits/Tops/OA_TshirtLngSlv/TshirtLngSlv/Materials/Crewneck/M_slv01_casuallng.M_slv01_casuallng"
MH_WALK = f"{MH}/Animations/AS_MH_Neutral_Walk_Loop_F.AS_MH_Neutral_Walk_Loop_F"
MH_IDLE = f"{MH}/Animations/AS_MH_Neutral_Stand_Idle_Loop.AS_MH_Neutral_Stand_Idle_Loop"
WALK_SPEED = 140  # cm/s, an unhurried walk
TOP_INFLATE, JEANS_INFLATE, SHOES_INFLATE = 1.06, 1.09, 1.04
PEOPLE_DIR = "/Game/LFF/People"
# Linear colours: Eagles midnight green #004C54, white, black; the print's white, silver #A5ACAF and outline.
TEES = {"Green": ((0.0, 0.045, 0.052), "EaglesHead"), "White": ((0.80, 0.80, 0.78), "EaglesHead"),
        "Black": ((0.012, 0.012, 0.014), "EaglesHead"), "Staff": ((0.012, 0.012, 0.014), "Staff")}
PRINT_COLOURS = ((0.85, 0.85, 0.83), (0.38, 0.41, 0.42), (0.004, 0.006, 0.007))
TEE_MATS = {}


def people_materials():
    """The print masks (SourceData/People) and one tee material per colour, in /Game/LFF/People."""
    for name in ("EaglesHead", "Staff"):
        tex = f"T_LFF_Print_{name}"
        # Imported once (the tees reference it, so it can't be deleted here): reimport in the editor after changing the PNG.
        if not call(AS, "exists", {"path": f"{PEOPLE_DIR}/{tex}"}):
            src = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "SourceData", "People", f"{tex}.png")
            call(TT, "import_file", {"folder_path": PEOPLE_DIR, "asset_name": tex, "source_file": src})
            set_props({"refPath": f"{PEOPLE_DIR}/{tex}.{tex}"}, {"SRGB": False, "CompressionSettings": "TC_Masks"})
            call(AS, "save_assets", {"asset_paths": [f"{PEOPLE_DIR}/{tex}"]})
    for colour, (rgb, print_name) in TEES.items():
        name = f"MI_LFF_Tee_{colour}"
        ref = {"refPath": f"{PEOPLE_DIR}/{name}.{name}"}
        if not call(AS, "exists", {"path": f"{PEOPLE_DIR}/{name}"}):
            call(MI, "create", {"folder_path": PEOPLE_DIR, "asset_name": name, "parent": {"refPath": MH_TEE_PARENT}})
        call(MI, "set_parent", {"instance": ref, "parent": {"refPath": MH_TEE_PARENT}})
        c = {"r": rgb[0], "g": rgb[1], "b": rgb[2], "a": 1}
        for p in ("diffuse_color_1", "diffuse_color_2", "B_diffuse_color_1", "C_color"):
            call(MI, "set_vector_parameter", {"instance": ref, "name": p, "value": c})
        call(MI, "set_static_switch_parameter", {"instance": ref, "name": "bDoPrintGraphic", "value": True})
        for switch in ("DoPaintedWear", "B_DoPaintedWear"):  # no worn patches: a new tee
            call(MI, "set_static_switch_parameter", {"instance": ref, "name": switch, "value": False})
        call(MI, "set_texture_parameter", {"instance": ref, "name": "PrintGraphicMap",
                                           "value": {"refPath": f"{PEOPLE_DIR}/T_LFF_Print_{print_name}.T_LFF_Print_{print_name}"}})
        for p, (r, g, b) in zip(("PrintGraphicColorA", "PrintGraphicColorB", "PrintGraphicColorC"), PRINT_COLOURS):
            call(MI, "set_vector_parameter", {"instance": ref, "name": p, "value": {"r": r, "g": g, "b": b, "a": 1}})
        for p, v in PRINT_PLACEMENT.items():
            call(MI, "set_scalar_parameter", {"instance": ref, "name": p, "value": v})
        call(AS, "save_assets", {"asset_paths": [f"{PEOPLE_DIR}/{name}"]})
        TEE_MATS[colour] = ref


# The print map spans the tee's UVs as laid out (Tools/Pipeline/prepare_people_prints.py places the art).
PRINT_PLACEMENT = {"PrintGraphicStrength": 1.0, "PrintScale": 1.0, "PrintGraphicOffsetU": 0.0, "PrintGraphicOffsetV": 0.0}


def metahuman(label, x, y, yaw, look, tee="Green", moving=None, z=None, folder=FOLDER_PROPS, tags=()):
    """A clothed person (ALFFPersonActor): body and face `look` (an index into MH_PEOPLE), tee, jeans and sneakers.
    It loops the idle and, while its cue moves it, the walk. Without a CueId it just stands."""
    name, sex = MH_PEOPLE[look % len(MH_PEOPLE)]
    p = Prop(label, x, y, yaw=yaw, cls=PERSON, z=z, folder=folder, tags=tags)
    base = f"{MH}/Characters/Assembled/{name}"
    values = {"BodyMesh": {"refPath": f"{base}/Body/SKM_MHC_{name}_BodyMesh.SKM_MHC_{name}_BodyMesh"},
              "FaceMesh": {"refPath": f"{base}/Face/SKM_MHC_{name}_FaceMesh.SKM_MHC_{name}_FaceMesh"},
              # Inflate: the cloth is only skinned, so the tee and jeans sit a few percent out to keep skin from showing.
              "Outfit": [{"Cloth": {"refPath": MH_TOP.format(sex)}, "Material": TEE_MATS[tee], "Inflate": TOP_INFLATE},
                         {"Cloth": {"refPath": MH_JEANS.format(sex)}, "Inflate": JEANS_INFLATE},
                         {"Cloth": {"refPath": MH_SHOES.format(sex)}, "Inflate": SHOES_INFLATE}],
              "StillAnimation": {"refPath": MH_IDLE}}
    if moving:
        values["MovingAnimation"] = {"refPath": moving}
    set_props(p.actor, values)
    return p


def security_checkpoint(gx, gy):
    """Step 05: one walk-through arch at the end of each lane of the line, and two people who walk through the middle
    one, one at a time, when the camera arrives (cue SecurityWalk): the line's front two, set up by queue_people."""
    security_arches(gx, gy)
    sign("LBL_Security", "Security", gx, gy - 650, 360, (gx - 700, gy - 1500, 300), 40)
    target("CT_Security", (gx - 700, gy - 1500), (gx, gy - 650, 150))


def security_arches(gx, gy):
    """Step 05: a walk-through metal detector at the end of each lane."""
    arches = Prop("SI_SecurityArches", gx, gy - 650, yaw=90)  # openings run along +Y: the line walks straight in
    for ax in (-300, 0, 300):  # the lane centres between the barrier lines (SI_QueueStanchions)
        arches.model("MetalDetector", (0, ax, 0))
    return arches


def bag_check_target(gx, gy):
    """Step 06: close on the clear bag on SI_BagTable (2.2 m away, a little above it); the people stay out of frame."""
    bag = (gx + 719, gy - 625)  # SI_BagTable's local (0, -40), turned to face (gx + 100, gy - 1450)
    cam = (gx + 584, gy - 799)  # back toward that old view point
    # Aimed at the bag's middle, 1 m above the ground under the camera: traced at the table, the aim point would
    # land on the table top and lift the view over the bag.
    target("CT_BagCheck", cam, (*bag, ground(*cam) + 100), eye=140, fov=35.0, look_z_is_absolute=True)


STAFF_AT, STAFF_CAM = (-7600, 1100), (-8400, 300)


def staff_attendant():
    """Step 22: a staff member in a black STAFF tee, facing the camera."""
    return metahuman("SI_StaffAttendant", *STAFF_AT, look((*STAFF_AT, 0), (*STAFF_CAM, 0))[1], 2, tee="Staff")


def queue_people(every=2):
    """The line at the Pepsi Plaza entry: clothed, idling people at every other point of Content/LFF/Crowd/queue.json
    (session 5's traced points; it replaces the Mass queue crowd, which stood in the bind pose). Tagged LFF_Crowd, so
    Crowd Off and Calm Mode hide them."""
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Content", "LFF", "Crowd", "queue.json")
    points = json.load(open(path))["seats"][::every]
    tees = ("Green", "Green", "White", "Black", "Green", "White")
    gx, gy = PLAZA_GATE
    walk = security_walkers(points)
    for i, (x, y, z, yaw) in enumerate(points):
        if i + 1 in walk:
            end_y, delay = walk[i + 1]
            run = end_y - y
            p = metahuman(f"SI_QueuePerson{i + 1}", gx, end_y, 90, i, tee=tees[i % len(tees)], moving=MH_WALK, z=z, folder=FOLDER_CROWD)
            p.cue("SecurityWalk", start_offset=(-run, 0, 0), seconds=run / WALK_SPEED, delay=delay, linear=True)
            continue
        metahuman(f"SI_QueuePerson{i + 1}", x, y, yaw, i, tee=tees[i % len(tees)], z=z, folder=FOLDER_CROWD, tags=("LFF_Crowd",))
    return len(points)


def security_walkers(points):
    """{line number: (end y, delay)}: the front two people of the middle lane (points 13 and 12, x = the middle arch)
    walk through the middle arch, one after the other. Their cue starts them at their place in line. Not tagged
    LFF_Crowd (the cue owns their visibility); Calm Mode and less movement show them past the arch, standing."""
    gx, gy = PLAZA_GATE
    first = 0.5
    second = first + (gy - 350 - points[12][1]) / WALK_SPEED + 1.0
    return {13: (gy - 350, first), 12: (gy - 500, second)}


def gate_flare_bursts():
    """Step 13 (gate flares around 11:50 AM, 12:00 PM and 12:15 PM): three short green aerial bursts in the sky over the
    north end of the bowl, seen from CT_GateFlares (cue GateFlareBursts). The soft fireworks actor: every spark keeps one
    steady glow, bursts 1.4 s apart, no strobe or flicker; none at all in Calm Mode (bSpectacle). The anthem keeps its
    own fireworks (SI_Fireworks)."""
    fw = Prop("SI_GateFlareBursts", 0, -6000, z=0, cls=FIREWORKS)
    set_props(fw.actor, {"BurstOffsets": [], "BurstColors": []})  # clear the class defaults first (arrays can't shrink and change at once)
    set_props(fw.actor, {"CueId": "GateFlareBursts", "Trigger": "Arrival", "PlayDelaySeconds": 0.8, "PlaySeconds": 5.5,
                         "bHiddenBefore": True, "bHiddenAfter": True, "bSpectacle": True, "SparkMaterial": GLOW,
                         "BurstOffsets": [{"x": -2500, "y": 0, "z": 9000}, {"x": 2200, "y": 800, "z": 9800},
                                          {"x": 0, "y": -1500, "z": 8600}],
                         "BurstColors": [{"r": 0.05, "g": 1.0, "b": 0.3, "a": 1}],
                         "SparksPerBurst": 50, "BurstRadius": 1300.0, "BurstSeconds": 2.5, "BurstInterval": 1.4})
    # From the plaza in front of the north gates, tilted up so the sky over the stand is in frame.
    target("CT_GateFlares", (-1500, -19500), (0, -6000, 7000), look_z_is_absolute=True, fov=70.0)


def clear_bag_material():
    """M_LFF_ClearBag: clear plastic. Translucent, two-sided, low roughness, 15 % opacity."""
    path = f"{MAT_DIR}/M_LFF_ClearBag"
    ref = {"refPath": f"{path}.M_LFF_ClearBag"}
    if call(AS, "exists", {"path": path}):
        try:
            used = call(AS, "get_referencers", {"asset_path": path})
        except Exception:  # noqa: BLE001 - the tool errors when nothing refers to it
            used = None
        if used:
            return ref  # already built and on the bag
        call(AS, "delete", {"path": path})  # half-built: start over
    ref = call(MT, "create_material", {"folder_path": MAT_DIR, "asset_name": "M_LFF_ClearBag"})
    set_props(ref, {"BlendMode": "BLEND_Translucent", "TranslucencyLightingMode": "TLM_SurfacePerPixelLighting", "TwoSided": True})
    for cls, values, prop in (("MaterialExpressionConstant3Vector", {"Constant": {"r": 0.92, "g": 0.95, "b": 0.97, "a": 1}}, "MP_BaseColor"),
                              ("MaterialExpressionConstant", {"R": 0.15}, "MP_Opacity"),
                              ("MaterialExpressionConstant", {"R": 0.05}, "MP_Roughness"),
                              ("MaterialExpressionConstant", {"R": 0.9}, "MP_Specular")):
        e = call(MT, "add_expression", {"material_or_function": ref, "expression_class": {"refPath": f"/Script/Engine.{cls}"}})
        set_props(e, values)
        outs = call(MT, "get_expression_output_names", {"expression": e}) or [""]
        call(MT, "connect_to_output", {"expression": e, "output_name": outs[0], "material_property": prop})
    call(MT, "recompile", {"material_or_function": ref})
    call(AS, "save_assets", {"asset_paths": [path]})
    return ref


def clear_bag_table(gx, gy):
    """Step 06: the bag check table and the clear bag model on it (a wallet, phone and headphones inside), its
    plastic in M_LFF_ClearBag."""
    table = Prop("SI_BagTable", gx + 750, gy - 650, face=(gx + 100, gy - 1450))
    table.box((70, 180, 85), (0, 0, 42), "Grey")
    table.model("Bag", (0, -40, 85), yaw=90, overrides={1: MATS["ClearBag"]})  # stood on the table, wide face out
    return table


def gate(label, cue_id, cx, cy, yaw, sign_id, sign_text, opening=440, fence_half=1100):
    """A gate proxy: posts, fence panels and a header, plus two leaves (ALFFCueActor) that slide open on cue_id's
    step. The leaves are placed open; they start closed. Less movement and Calm Mode show them already open."""
    frame = Prop(f"{label}_Frame", cx, cy, yaw=yaw)
    half = opening / 2
    for side in (-1, 1):
        frame.box((24, 24, 300), (0, side * (half + 14), 150), "Grey")                    # tall gate posts
        length = fence_half - half - 26
        frame.box((5, length, 110), (0, side * (half + 26 + length / 2), 55), "Grey")     # fence panel
        frame.box((8, length, 8), (0, side * (half + 26 + length / 2), 112), "Dark")      # top rail
    frame.box((24, opening + 52, 24), (0, 0, 300), "Grey")                                 # header beam
    sx, sy, _ = frame.local(0, 0)
    sign(sign_id, sign_text, sx, sy, 380, frame.local(1500, 0, 300), 34)  # faces the approach (+X)
    for side in (-1, 1):
        # Closed: centre at +-half/2. Open (rest): slid behind the fence on the far side.
        ox, oy, _ = frame.local(-14, side * (half / 2 + half))
        leaf = Prop(f"{label}_Leaf{'L' if side < 0 else 'R'}", ox, oy, yaw=yaw, cls=CUE, z=frame.z)
        leaf.box((6, half - 6, 8), (0, 0, 6), "Grey")
        leaf.box((6, half - 6, 8), (0, 0, 206), "Grey")
        for by in (-(half / 2 - 6), half / 2 - 6):
            leaf.box((6, 8, 208), (0, by, 106), "Grey")
        for i in range(1, 6):
            leaf.box((4, 4, 200), (0, -half / 2 + i * half / 6, 106), "Dark")
        leaf.cue(cue_id, start_offset=(0, -side * half, 0), seconds=3.5)
    return frame


def popcorn_cart(x, y, face):
    """Popcorn for the People (from the brief's photo description): a white cart under a black-and-white striped
    awning, a wall of red popcorn boxes behind the counter, and a header: Wawa | POPCORN FOR THE PEOPLE |
    Eagles Autism Foundation. Wawa and EAF are their logos from Assets/Logos; the title stays lettering."""
    k = Prop("SI_PopcornCart", x, y, face=face)
    for wx in (-45, 45):
        for wy in (-130, 130):
            k.cyl(18, 10, (wx, wy, 18), "Dark", rot=(90, 0, 0))
    k.box((120, 320, 95), (0, 0, 75), "Paint")                   # white cart body
    k.box((140, 340, 6), (0, 0, 126), "Bone")                    # counter
    k.box((10, 330, 160), (-62, 0, 210), "Paint")                # back wall
    for row in range(3):                                         # red popcorn-box wall, a white stripe each
        for col in range(7):
            by, bz = -135 + col * 45, 152 + row * 48
            k.box((18, 34, 42), (-48, by, bz), "Red")
            k.box((19, 6, 42), (-48, by, bz), "Paint")
            k.ball(12, (-48, by, bz + 26), "Bone")
    for px in (-58, 58):
        for py in (-162, 162):
            k.cyl(3, 170, (px, py, 213), "Dark")                 # awning posts
    for i in range(9):                                           # striped awning, sloping down to the front
        sy = -160 + i * 40
        mat = "Dark" if i % 2 == 0 else "Paint"
        k.box((180, 40, 5), (10, sy, 296), mat, rot=(-10.6, 0, 0))
        k.box((3, 40, 22), (100, sy, 268), mat)                  # front valance
    k.box((6, 340, 78), (-66, 0, 350), "Paint")                  # header board
    # The title across the top, then Wawa on the left and EAF on the right, as the reference reads left to right
    # (the viewer faces -X, so their left is +Y). The logos sit on planes just proud of the header board,
    # sized to their canvases (prepare_logos.py): Wawa 2:1, EAF 4:1 (the logo fills 72% of its width).
    painted_text(k, "POPCORN FOR THE PEOPLE", (-62, 0, 368), 20, INK, "TXT_Popcorn_Title")
    k.logo(LOGO_WAWA, (-62.5, 90, 331), 64, 32)
    k.logo(LOGO_EAF, (-62.5, -75, 331), 128, 32)
    return k


def sensory_trailer(x, y, face):
    """Sensory Activation Vehicle: the trailer model (MODELS) in the real trailer's wrap (lettering, the green MOBILE
    SENSORY SPACE stripe, the KultureCity, CHOP and Eagles Autism Foundation logos, the door's EAF panel), door side
    toward the camera, and the white rooftop air conditioner from the photo. Nothing on it moves."""
    v = Prop("SI_SensoryTrailer", x, y, face=face)
    v.model("SensoryVehicle", (0, 0, 0))
    v.box((70, 75, 26), (0, 83, 246), "Paint")        # rooftop AC, a little behind the middle (roof at 233 cm)
    for gy in (-22, -11, 0, 11, 22):                   # its grille
        v.box((72, 4, 14), (0, 83 + gy, 246), "Grey")
    return v


SAV_AT, SAV_FACE = (-8300, -20400), (-7950, -21350)  # the trailer, and the point its door side faces
SAV_CAM = (-8055, -21065)                             # 7 m out on that line: the 4.1 m trailer fills the frame


def sensory_vehicle_target():
    return target("CT_SensoryVehicle", SAV_CAM, (*SAV_AT, 130))


def queue_barriers(gx, gy):
    """Step 04: the line's four lane lines at the Pepsi Plaza entry, each four bike-rack barrier sections end to end."""
    queue = Prop("SI_QueueStanchions", gx, gy - 1900)
    for x in (-450, -150, 150, 450):
        for i in range(4):
            queue.model("Barrier", (x, 137.5 + i * 275, 0), yaw=90)
    return queue


def food_stand():
    """Step 22: the concession stand model, under the LBL_Food sign, its counters facing the concourse."""
    stand = Prop("SI_FoodCounter", -7320, -1200, yaw=180)
    stand.model("Concession", (0, 0, 0), yaw=90)
    return stand


def food_sign():
    """Moves the original LBL_Food (build_lff_stadium_level.py) to just in front of the stand's front (1.35 m out
    from its centre) and above its 3.6 m roof, facing CT_Food."""
    food = next(a for a in call(SC, "find_actors", {"name": "LBL_Food", "tag": "LBL_Food", "collision_channels": []}))
    fz = ground(-7320, -1200) + 420
    call(AT, "set_actor_transform", {"actor": food, "worldspace": False,
                                     "xform": xform((-7480, -1200, fz), (0, look((-7480, -1200, fz), (-8350, -2300, 0))[1], 0))})


def midfield_logo():
    """The Eagles logo painted at midfield, as on the real field: a decal projected down onto the turf, so it
    follows the model and can't z-fight from the low seat views. Always on (it's part of the field, not a cue);
    the seat views (walk to seats, lights, anthem, kickoff, finale) all look across it. Image top points away from
    the west-stand seat, so it reads upright from there. About 13 m along the field (the head is 1.41:1)."""
    z = ground(0, 0)
    a = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": "/Script/Engine.DecalActor"}, "name": "SI_MidfieldLogo",
                                            "xform": xform((0, 0, z), (-90, 90, 0)), "parent": SITE_ROOT, "snap_to_ground": False})
    # Projecting down, the decal's +Z runs along the texture's width; yaw 90 puts the logo's top toward the east
    # stand, upright from the west-stand seat (checked from overhead against the source art).
    call(AT, "set_actor_transform", {"actor": a, "worldspace": False, "xform": xform((0, 0, z), (-90, 90, 0))})
    call(AT, "set_label", {"actor": a, "label": "SI_MidfieldLogo"})
    call(AT, "add_tag", {"actor": a, "tag": TAG})
    call(SC, "set_actor_folder", {"actor": a, "folder_path": FOLDER_PROPS})
    decal = [c for c in call(AT, "get_components", {"actor": a, "component_type": None}) if "Decal" in c["refPath"]][0]
    # DecalSize is a half-extent: X is the projection depth (60 cm each way, so only the turf takes it), Z the
    # texture's width and Y its height. The canvas is 2:1 and the logo fills 1449/2048 of its width.
    along = 1300 / (1449 / 2048)
    set_props(decal, {"DecalMaterial": {"refPath": LOGO_MIDFIELD}, "DecalSize": {"x": 60, "y": along / 4, "z": along / 2}})
    return a


def remove_previous():
    removed = 0
    for a in call(SC, "find_actors", {"name": "", "tag": TAG, "collision_channels": []}) or []:
        call(SC, "remove_from_scene", {"actor": a})
        removed += 1
    print(f"removed {removed} actors from the previous run")


def geometry_cache_child(parent, cache, rel_yaw=0.0):
    """The eagle's flap loop (GeometryCache) on a GeometryCacheActor attached to its flight (a cue actor)."""
    # With a parent, the transform is relative to it: the mesh rides at the flight's origin.
    a = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": GEOMETRY_CACHE_ACTOR}, "name": "SI_EagleMesh",
                                            "xform": xform((0, 0, 0), (0, rel_yaw, 0)),
                                            "parent": parent.actor, "snap_to_ground": False})
    call(AT, "set_label", {"actor": a, "label": "SI_EagleMesh"})
    call(AT, "add_tag", {"actor": a, "tag": TAG})
    call(SC, "set_actor_folder", {"actor": a, "folder_path": FOLDER_PROPS})
    comp = [c for c in call(AT, "get_components", {"actor": a, "component_type": None}) if "GeometryCache" in c["refPath"]][0]
    set_props(comp, {"GeometryCache": {"refPath": cache}, "bLooping": True, "bRunning": True, "CastShadow": False,
                     "Mobility": "Movable"})
    return a


def main():
    global SITE_ROOT, MATS
    SITE_ROOT = next(a for a in call(SC, "find_actors", {"name": "LFF_SiteRoot", "tag": "LFF_SiteRoot", "collision_channels": []}))
    remove_previous()
    MATS = {name: material(name) for name in COLOURS}
    MATS["ClearBag"] = clear_bag_material()
    people_materials()
    counts = {"cars": 0, "people": 0, "cue actors": 0}

    # --- 02. The wide shot the splash orbits through: from the south, the stadium with Center City beyond. ------
    # Looks at the point above midfield that the orbit keeps in frame (ULFFJourneySubsystem::GetSplashOrbit).
    target("CT_StadiumWide", WIDE_CAM[:2], (0, 0, WIDE_LOOK_Z), look_z_is_absolute=True, cam_z=WIDE_CAM[2], fov=60.0)

    # --- 03. Parking: two rows of parked cars; one more rolls into the empty bay when the camera arrives. -------
    # City Sample cars, the five models in turn, so no two neighbours match.
    paints = ("Silver", "White", "Black", "Grey", "Navy", "White", "Red", "Silver", "Beige", "Black")
    row_y = [-24480 + k * 290 for k in range(6)]
    arrival_bay = row_y[1]
    n = 0
    for x, yaw in ((2600, 0), (3400, 180)):
        for y in row_y if x == 2600 else row_y[:5]:
            if x == 2600 and y == arrival_bay:
                continue
            city_car(f"SI_Car{n + 1}", x, y, CITY_CARS[n % len(CITY_CARS)], paints[n % len(paints)], face_yaw=yaw)
            n += 1
    counts["cars"] = n
    arrival = city_car("SI_CarArrival", 2600, arrival_bay, "05", "Green", face_yaw=0, cls=CUE)
    arrival.cue("CarArrival", start_offset=(-2600, 0, 0), seconds=6.0)  # rolls in forward and stops; off-frame at first
    counts["cue actors"] += 1
    bays = Prop("SI_ParkingBays", 2600, -23900)
    for dy in range(-725, 1016, 290):  # lines between the six bays of the first row
        bays.box((520, 10, 2), (0, dy, 1), "Paint")
    sign("LBL_Parking", "Parking", 3000, -23100, 420, (900, -22900, 300), 50)
    # Framed so the arriving car ends right of centre, clear of the caption card; it enters from off-frame left.
    target("CT_Parking", (1300, -22600), (2600, -24000, 110))

    # --- 04-08. The Pepsi Plaza entry, north of the plaza: line, security, the gate, clear bags, ticket. ---------
    gx, gy = PLAZA_GATE
    queue_barriers(gx, gy)
    for i, (qx, qy, shirt, pants) in enumerate(((-300, -1700, "Blue", "Dark"), (-300, -1450, "Grey", "Midnight"),
                                                (0, -1600, "Bone", "Dark"), (0, -1300, "Midnight", "Grey"),
                                                (300, -1750, "Red", "Dark"), (300, -1500, "Silver", "Midnight"))):
        person(f"SI_CrowdLine{i + 1}", gx + qx, gy + qy, 90, shirt, pants, scale=0.97 + 0.02 * (i % 3))
        counts["people"] += 1
    counts["people"] += queue_people()
    sign("LBL_Line", "Line", gx - 700, gy - 1300, 300, (gx - 1100, gy - 2700, 300), 34)
    target("CT_Line", (gx - 1100, gy - 2700), (gx, gy - 1200, 120))

    security_checkpoint(gx, gy)

    gate("SI_PlazaGate", "GatePlaza", gx, gy, -90, "LBL_PlazaGate", "Pepsi Plaza gates")
    target("CT_PlazaGate", (gx - 1300, gy - 1150), (gx, gy, 170))

    clear_bag_table(gx, gy)
    sign("LBL_ClearBag", "Clear bags only", gx + 750, gy - 650, 290, (gx + 100, gy - 1450, 300), 30)
    bag_check_target(gx, gy)

    # The phone itself is on screen (the HUD raises it); here the view is the open gate and the staff member.
    staff = person("SI_TicketStaff", gx + 350, gy - 150, 90 + 180, "Midnight", "Dark", crowd=False)
    staff.cyl(12, 6, (0, 0, 173), "Midnight")  # cap
    target("CT_Ticket", (gx - 250, gy - 800), (gx + 350, gy - 150, 150))  # the staff member, centred

    # --- 09-11. The plaza: people, the popcorn cart and the sensory trailer. -----------------------------------
    target("CT_Plaza", (-9300, -21900), (-3000, -18600, 250))
    for i, (px, py, yaw, shirt, pants) in enumerate(((-5200, -20600, 30, "Grey", "Blue"), (-6000, -19800, 200, "Bone", "Dark"),
                                                    (-4600, -21200, 120, "Midnight", "Grey"), (-6800, -21000, 300, "Red", "Dark"))):
        person(f"SI_CrowdPlaza{i + 1}", px, py, yaw, shirt, pants)
        counts["people"] += 1

    pop_at, pop_cam = (-3700, -19600), (-3700, -20550)
    popcorn_cart(*pop_at, face=pop_cam)
    for i, (px, py, shirt, pants) in enumerate(((-4180, -20000, "Blue", "Grey"), (-3230, -20050, "Silver", "Dark"),
                                                (-3330, -19900, "Bone", "Midnight"))):  # to the sides, not in front
        person(f"SI_CrowdPopcorn{i + 1}", px, py, look((px, py, 0), (*pop_at, 0))[1], shirt, pants)
        counts["people"] += 1
    target("CT_Popcorn", pop_cam, (*pop_at, 220))

    sav_at = SAV_AT
    sensory_trailer(*sav_at, face=SAV_FACE)
    for i, (px, py, shirt, pants) in enumerate(((-7500, -20750, "Grey", "Dark"), (-8950, -20950, "Midnight", "Grey"))):
        person(f"SI_CrowdSAV{i + 1}", px, py, look((px, py, 0), (*sav_at, 0))[1], shirt, pants)
        counts["people"] += 1
    sensory_vehicle_target()

    # --- 12-14. Club gates at 11:00, inner stadium gates at 11:30 (flares are viewed from there too). ----------
    cx, cy = CLUB_GATE
    gate("SI_ClubGate", "GateClub", cx, cy, -90, "LBL_ClubGate", "Club gates", opening=360, fence_half=700)
    target("CT_ClubGate", (cx - 450, cy - 1100), (cx, cy, 170))
    gate("SI_InnerGate", "GateInner", 0, -14450, -90, "LBL_InnerGate", "Inner gates")
    # Closer than CT_GateEntry (build_lff_stadium_level.py), so the gate reads; the flares step uses it too.
    target("CT_InnerGate", (-800, -15900), (0, -14450, 180))
    gate_flare_bursts()

    # --- 12. Bright lights: the seat view, tilted up toward the east roof and its lights. ---------------------
    # Same seat as CT_SeatFieldView (5.2 m above midfield turf, build_lff_stadium_level.py), looking up.
    target("CT_SeatLights", (-5050, 0), (6000, 0, 3900), look_z_is_absolute=True, cam_z=520)

    # --- The field: the Eagles logo at midfield, in front of the seat (finale, kickoff and the other seat views). --
    midfield_logo()

    # --- 18. Anthem: the same seat looking a little up, so the eagle's pass and the fireworks are in frame. ----
    target("CT_Anthem", (-5050, 0), (6000, 0, 2500), look_z_is_absolute=True, cam_z=520)
    # One pass along the field, 20 m in front of the seat and about 9 m above eye level (above the caption card
    # in the frame), entering and leaving off-frame. Placed at its end; it starts 80 m back. Preview only.
    flight = Prop("SI_EagleFlight", -3000, 4000, yaw=90, z=1400, cls=CUE)
    flight.cue("Anthem", start_offset=(-8000, 0, 0), seconds=9.0, delay=0.5, trigger="Preview", linear=True,
               hidden_before=True, hidden_after=True)
    geometry_cache_child(flight, EAGLE_CACHE, rel_yaw=EAGLE_MESH_YAW)
    fw = Prop("SI_Fireworks", 7000, 0, yaw=180, z=4800, cls=FIREWORKS)
    set_props(fw.actor, {"CueId": "Anthem", "PlayDelaySeconds": 3.0, "SparkMaterial": GLOW})
    counts["cue actors"] += 2

    # --- 20-23. On the plaza-level concourse (open-air, west side): Guest Services, sensory room, food, staff. ---
    # The concourse walls have no collision, so these stand free a little way out from the inner wall.
    gs_at, gs_cam = (-7550, 3300), (-8350, 2200)
    booth = Prop("SI_GuestServices", *gs_at, face=gs_cam)
    booth.box((70, 260, 105), (0, 0, 52), "Midnight")
    booth.box((80, 280, 6), (0, 0, 108), "Bone")
    booth.box((20, 280, 240), (-60, 0, 120), "Bone")
    sign("LBL_GuestServices", "Guest Services<br>Sections 121 · 136 · 206 · 226", *gs_at, 330, (*gs_cam, 300), 30, lines=2)
    target("CT_GuestServices", gs_cam, (*gs_at, 150))

    # The sensory room sits at "Actor - Sensory Room Location" (placed by hand, 2026-10-07): SI_SensoryRoomProxy (the room,
    # hand-built, centre (-8027, 4379), turned 180 so its doorway faces Guest Services) and CP_ConcourseToSensoryRoom (the
    # route in) are in the level, not here. These pieces keep their old places relative to the room.
    door_at, room_cam = (-8457, 3859), (-8027, 4059)
    door = Prop("SI_SensoryRoomDoor", *door_at, yaw=0)  # a free-standing portal beside the doorway
    door.box((20, 20, 230), (0, -75, 115), "Grey")
    door.box((20, 20, 230), (0, 75, 115), "Grey")
    door.box((20, 170, 20), (0, 0, 240), "Grey")
    door.box((6, 130, 220), (-4, 0, 110), "Blue")
    door.box((4, 8, 30), (-9, -45, 105), "Silver")
    # On the room's back wall, facing the camera inside the doorway; the camera looks in and a little down (12 degrees).
    sign("LBL_SensoryRoom", "Sensory Room<br>West Lower Suite Level", -8027, 4754, 305, (*room_cam, 463), 28, lines=2)
    target("CT_SensoryRoom", room_cam, (-8027, 4959, -27))

    food_stand()  # under the existing LBL_Food sign
    target("CT_Food", (-8350, -2300), (-7320, -1200, 170))

    staff_at, staff_cam = STAFF_AT, STAFF_CAM
    staff_attendant()
    sign("LBL_Staff", "Staff", *staff_at, 285, (*staff_cam, 300), 20)
    target("CT_Staff", staff_cam, (*staff_at, 120))

    # --- Move the original plaza sign above the plaza, out of the lower third. ---------------------------------
    plaza = next(a for a in call(SC, "find_actors", {"name": "LBL_Plaza", "tag": "LBL_Plaza", "collision_channels": []}))
    pz = ground(-4600, -18200) + 900
    call(AT, "set_actor_transform", {"actor": plaza, "worldspace": False,
                                     "xform": xform((-4600, -18200, pz), (0, look((-4600, -18200, pz), (-9300, -21900, 0))[1], 0))})

    # ...and the original food sign off the concourse wall (it was partly inside it), over the concession stand.
    food_sign()

    call(AS, "save_assets", {"asset_paths": [LEVEL]})
    print(f"placed stand-ins, labels and targets; level saved. {counts['cars']} parked cars + 1 arriving, "
          f"{counts['people']} crowd stand-ins, {counts['cue actors']} + 6 gate leaves cue actors")


# Layout constants (local stadium frame, cm). The wide shot sits on the splash orbit.
WIDE_CAM = (0, 40000, 13500)       # 400 m south of midfield, 135 m up
WIDE_LOOK_Z = 3000                 # aim a little above the turf, so Center City shows above the north stand
PLAZA_GATE = (-1500, -22800)       # the Pepsi Plaza entry, between the parking lot and the plaza
CLUB_GATE = (-4200, -15200)        # beside the north headhouse
EAGLE_MESH_YAW = 0.0               # the baked eagle already faces +X (Tools/Pipeline/prepare_eagle.py)


if __name__ == "__main__":
    main()
