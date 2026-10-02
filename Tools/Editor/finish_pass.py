"""The finish pass on L_LFF_Stadium (session 4), through Unreal MCP. Run from the repo root with the editor open on
L_LFF_Stadium, the MCP server running and PIE stopped:
  python3 Tools/Editor/finish_pass.py                 # every step, then saves the level
  python3 Tools/Editor/finish_pass.py lot bowl        # only those steps
Needs Pillow and numpy. Safe to rerun: each step removes what it placed before (tag LFF_FinishPass) or sets the same
values again. Rerun it after Tools/Editor/build_lff_standins.py, which rebuilds the stand-ins it adjusts.

Steps
  sprites   Editor icons off everywhere: the billboard on every plain actor, the "Tt" sprite on every TextRenderActor
            and the camera targets' arrows (all are already hidden in game; this hides them in the editor viewport and
            in captures too, so no icon shows in any view).
  parking   CT_Parking 4 m further back, 4 m up and turned 2 deg right, so all 11 City Sample cars (and the arriving
            car's empty bay) are in the 70 deg frame, clear of the caption card at lower left, the back row over the
            front row.
  people    The plaza's primitive people (SI_CrowdPlaza*, SI_CrowdPopcorn*, SI_CrowdSAV*) read as grey-headed bollards
            from the plaza cameras, so they are hidden in game (tag LFF_Crowd removed, so Crowd Low can't show them).
            The line and security people and the staff stay.
  lot       Fills the model's whole asphalt lot around the stadium with City Sample cars: one actor (SI_LotCars) of
            instanced static meshes, a body per model and colour plus wheels and glass per model. Bays line up with the
            parking step's two hero rows; the arriving car's lane, every step camera's near view, the stand-ins and
            the hero rows' sight lines stay clear. Where the lot is: a top-down capture of the site, asphalt told from
            walkways and shadow by colour (Saved/LotMask). Every car is traced onto the ground. The lot and the ten
            parked hero cars are tagged LFF_Exterior: ULFFJourneySubsystem hides them while the camera is inside the
            stadium (box: LFF_StadiumInterior, placed here too).
  ticket    Re-imports the phone ticket frames (Tools/Pipeline/prepare_ticket.py: Section 111, Row 10, Seat 8; Eagles
            vs. Carolina Panthers, Sun, October 18).
  bowl      A thin band of seated fans in the lower bowl, seen from the aerial and kickoff cameras: instanced crowd
            cards (one painted row of fans in midnight green, white, silver and black; Tools/Pipeline/prepare_crowd.py)
            standing upright on five front rows, 200 at most, tagged LFF_Crowd so Crowd Off and Calm Mode hide them. No
            AI, Mass or animation.
Local coordinates are the stadium frame (Tools/Editor/build_lff_stadium_level.py): +X across the field toward the east
stand, +Y along it toward the south end, cm; LFF_SiteRoot sits at the world origin turned -6.5 deg.
"""
import base64
import io
import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(__file__))
from unreal_mcp import call  # noqa: E402
import build_lff_standins as standins  # noqa: E402  (car models, paint MIs; importing it builds nothing)

SC = "editor_toolset.toolsets.scene.SceneTools"
AT = "editor_toolset.toolsets.actor.ActorTools"
OT = "editor_toolset.toolsets.object.ObjectTools"
AS = "editor_toolset.toolsets.asset.AssetTools"
EA = "EditorToolset.EditorAppToolset"
PG = "editor_toolset.toolsets.programmatic.ProgrammaticToolset"

LEVEL = "/Game/LFF/Levels/L_LFF_Stadium"
TAG = "LFF_FinishPass"
EXTERIOR = "LFF_Exterior"
ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
MASK_DIR = os.path.join(ROOT, "Saved", "LotMask")
SITE_HEADING_DEG = -6.5
LOT_Z = (125.0, 165.0)  # the model's lot is flat at about +1.43 m; anything else under a car is something else


def world_xy(x, y):
    c, s = math.cos(math.radians(SITE_HEADING_DEG)), math.sin(math.radians(SITE_HEADING_DEG))
    return x * c - y * s, x * s + y * c


def local_xy(x, y):
    c, s = math.cos(math.radians(-SITE_HEADING_DEG)), math.sin(math.radians(-SITE_HEADING_DEG))
    return x * c - y * s, x * s + y * c


def set_props(obj, values):
    if not call(OT, "set_properties", {"instance": obj, "values": json.dumps(values)}):
        raise RuntimeError(f"set_properties failed on {obj['refPath']}: {list(values)}")


def all_actors():
    return {call(AT, "get_label", {"actor": a}): a for a in call(SC, "find_actors", {"name": "", "tag": "", "collision_channels": []})}


def remove_previous(kind):
    for a in call(SC, "find_actors", {"name": "", "tag": f"{TAG}_{kind}", "collision_channels": []}):
        call(SC, "remove_from_scene", {"actor": a})


def spawn(label, kind, folder, tags=(), loc=(0, 0, 0)):
    """A plain actor under LFF_SiteRoot at a local position (its billboard hidden)."""
    root = next(iter(call(SC, "find_actors", {"name": "LFF_SiteRoot", "tag": "LFF_SiteRoot", "collision_channels": []})))
    a = call(SC, "add_to_scene_from_class", {"actor_type": {"refPath": "/Script/Engine.Actor"}, "name": label,
                                             "xform": {"location": dict(zip("xyz", loc))}, "parent": root, "snap_to_ground": False})
    call(AT, "set_label", {"actor": a, "label": label})
    for t in (TAG, f"{TAG}_{kind}", *tags):
        call(AT, "add_tag", {"actor": a, "tag": t})
    call(SC, "set_actor_folder", {"actor": a, "folder_path": folder})
    hide_icons(a)
    return a


def hide_icons(actor):
    for c in call(AT, "get_components", {"actor": actor}):
        name = c["refPath"].rsplit(".", 1)[-1]
        if name.startswith("BillboardComponent") or name in ("Sprite", "DirectionArrow"):
            set_props(c, {"bVisible": False, "bHiddenInGame": True})


def trace_down(points, top=600.0, depth=900.0):
    """Ground height under each world (x, y), or None. Runs in the editor in batches (one MCP call per 300)."""
    out = []
    for i in range(0, len(points), 300):
        batch = [[round(x, 1), round(y, 1)] for x, y in points[i:i + 300]]
        script = ("import json\n"
                  f"P = {json.dumps(batch)}\n"
                  "def run():\n"
                  "    out = []\n"
                  "    for x, y in P:\n"
                  "        r = execute_tool('editor_toolset.toolsets.scene.SceneTools.trace_world', json.dumps({\n"
                  f"            'start': {{'x': x, 'y': y, 'z': {top}}}, 'end': {{'x': x, 'y': y, 'z': {top - depth}}}}}))\n"
                  "        d = r.get('returnValue') if hasattr(r, 'get') else None\n"
                  f"        out.append(None if d is None else {top} - d)\n"
                  "    return {'z': out}\n")
        res = call(PG, "execute_tool_script", {"script": script})
        out += (json.loads(res) if isinstance(res, str) else res)["z"]
    return out


def capture(x, y, z, pitch, yaw):
    """A 90 deg capture from a world pose, as a PIL image (taken twice: the first frame's occlusion is stale)."""
    from PIL import Image
    args = {"captureTransform": {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": pitch, "yaw": yaw, "roll": 0},
                                 "scale": {"x": 1, "y": 1, "z": 1}},
            "annotations": {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0, "maxLabelDistance": 0, "classFilter": None, "maxLabels": 0},
            "bShowUI": False}
    for _ in range(2):
        cap = call(EA, "CaptureViewport", args)
    if abs(cap["cameraFOV"] - 90.0) > 0.01:
        raise RuntimeError(f"expected a 90 deg viewport, got {cap['cameraFOV']}")
    return Image.open(io.BytesIO(base64.b64decode(cap["image"]["data"]))).convert("RGB")


# --- sprites -----------------------------------------------------------------------------------------------------------
def step_sprites():
    n = 0
    for label, a in all_actors().items():
        for c in call(AT, "get_components", {"actor": a}):
            name = c["refPath"].rsplit(".", 1)[-1]
            if name.startswith("BillboardComponent") or name in ("Sprite", "DirectionArrow"):
                set_props(c, {"bVisible": False, "bHiddenInGame": True})
                n += 1
    print(f"sprites: {n} editor icons hidden")


# --- parking -----------------------------------------------------------------------------------------------------------
PARKING_CAM = (1028, -22307)   # local; 4 m behind the old pose along its view
PARKING_EYE = 400              # above the lot, so the back row reads over the front row
PARKING_YAW, PARKING_PITCH = -43.1, -7.0  # local: 2 deg right of the old aim


def step_parking():
    ct = all_actors()["CT_Parking"]
    wx, wy = world_xy(*PARKING_CAM)
    z = trace_down([(wx, wy)])[0] + PARKING_EYE
    call(AT, "set_actor_transform", {"actor": ct, "xform": {"location": {"x": wx, "y": wy, "z": z},
                                                              "rotation": {"pitch": PARKING_PITCH, "yaw": PARKING_YAW + SITE_HEADING_DEG, "roll": 0}}})
    print(f"parking: CT_Parking at local {PARKING_CAM}, {PARKING_EYE / 100:.1f} m up, pitch {PARKING_PITCH}, local yaw {PARKING_YAW}")


# --- people ------------------------------------------------------------------------------------------------------------
PLAZA_PEOPLE = ("SI_CrowdPlaza", "SI_CrowdPopcorn", "SI_CrowdSAV")


def step_people():
    n = 0
    for label, a in all_actors().items():
        if label.startswith(PLAZA_PEOPLE):
            if call(AT, "has_tag", {"actor": a, "tag": "LFF_Crowd"}):
                call(AT, "remove_tag", {"actor": a, "tag": "LFF_Crowd"})
            if not call(AT, "has_tag", {"actor": a, "tag": "LFF_HiddenStandIn"}):
                call(AT, "add_tag", {"actor": a, "tag": "LFF_HiddenStandIn"})
            set_props(a, {"bHidden": True})
            n += 1
    print(f"people: {n} plaza stand-in people hidden in game")


# --- lot ---------------------------------------------------------------------------------------------------------------
MASK_RES = 50.0                      # cm per mask cell
MASK_X, MASK_Y = (-28000, 28000), (-45000, 45000)
TILE_ALT = 30000.0                   # capture height over the lot; 90 deg => 31.75 cm per pixel
TILES = [(lx, ly) for lx in (-11000, 11000) for ly in (-14000, 14000)]
# The hero rows (build_lff_standins.py): cars at x = 2600 (facing +X) and 3400 (facing -X), bays 290 cm apart from
# y = -24480; the fill carries that double row on along the lot. Every other double row is a real lot's: 2.7 m bays,
# noses 80 cm apart, 18 m from one double row to the next (7.2 m aisles, 6.4 m beside the hero pair).
HERO_ROW = (3000, 400, 290)          # centre x, half-spacing, bay (cm)
ROW_PITCH, ROW_HALF, BAY = 1800, 280, 270
BAY0 = -24480
CROSS_AISLE = (20, (10, 11))         # every 20 bays, two bays left empty as a cross aisle
EMPTY_BAYS = 0.05                    # share of bays left empty at random, as in a real lot
CAR_HALF = (290, 130)                # footprint half-size (with margin) that must be all asphalt
LOT_STRIP = 600                      # asphalt narrower than twice this is a road (the perimeter road), not parking
LOT_MARGIN = 100                     # kerb margin around each car
LOT_MIN_AREA = 400                   # m2: smaller pockets are left empty
ARRIVAL = (-24190, (-300, 2450))     # the arriving car's lane: its bay's y, from its start x to its bay
PAINT_MIX = {"White": 22, "Black": 18, "Silver": 18, "Grey": 16, "Navy": 8, "Red": 7, "Beige": 6, "Green": 5}
MAX_CARS = 1400
SEED = 1018


def build_mask():
    """Asphalt cells over the lot, from four top-down captures in the local frame. Cached in Saved/LotMask."""
    import numpy as np
    from PIL import Image, ImageFilter
    path = os.path.join(MASK_DIR, "lot_mask.npy")
    if os.path.exists(path):
        return np.load(path)
    os.makedirs(MASK_DIR, exist_ok=True)

    def box(v, r):
        k = 2 * r + 1
        c = np.pad(np.pad(v, r, mode="edge").cumsum(0).cumsum(1), ((1, 0), (1, 0)))
        return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)

    h, w = int((MASK_X[1] - MASK_X[0]) / MASK_RES), int((MASK_Y[1] - MASK_Y[0]) / MASK_RES)
    gy, gx = np.mgrid[0:h, 0:w]
    lx, ly = MASK_X[1] - (gy + 0.5) * MASK_RES, MASK_Y[0] + (gx + 0.5) * MASK_RES
    mask, cover = np.zeros((h, w), bool), np.zeros((h, w), bool)
    for cx, cy in TILES:
        wx, wy = world_xy(cx, cy)
        img = capture(wx, wy, TILE_ALT + 150, -90, SITE_HEADING_DEG)  # image up = local +X, right = local +Y
        img.save(os.path.join(MASK_DIR, f"tile_{cx}_{cy}.png"))
        a = np.asarray(img).astype(float)
        m, spread = a.mean(2), a.max(2) - a.min(2)
        sd = np.sqrt(np.maximum(box(m ** 2, 3) - box(m, 3) ** 2, 0))
        # Asphalt is flat neutral grey: the walkways are warm, shadow is blue, the Google tiles are textured. Each
        # capture has its own exposure, so the asphalt level is the commonest flat neutral grey in it.
        neutral = (spread < 12) & (sd < 6) & (m > 40)
        hist, edges = np.histogram(m[neutral], bins=64, range=(40, 200))
        level = edges[hist.argmax()] + 1.25
        tile = neutral & (abs(m - level) < 16)
        tile = np.asarray(Image.fromarray((tile * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(5))
                          .filter(ImageFilter.MaxFilter(3))) > 127
        s = 2 * TILE_ALT / img.width
        py = np.round((img.height - 1) / 2 - (lx - cx) / s).astype(int)
        px = np.round((img.width - 1) / 2 + (ly - cy) / s).astype(int)
        ok = (py >= 0) & (py < img.height) & (px >= 0) & (px < img.width) & (abs(ly - cy) <= 15000) & ~cover
        mask[ok] = tile[py[ok], px[ok]]
        cover |= ok
    np.save(path, mask)
    return mask


def erode(mask, cm, dilate=False):
    """Asphalt cells at least `cm` from anything else (or, dilating, within `cm` of asphalt)."""
    import numpy as np
    from PIL import Image, ImageFilter
    k = 2 * int(round(cm / MASK_RES)) + 1
    f = ImageFilter.MaxFilter(k) if dilate else ImageFilter.MinFilter(k)
    return np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(f)) > 127


def parking_mask(mask):
    """The lot without its thin roads (an opening: erode, then grow back, only where it was asphalt), less a kerb."""
    opened = erode(erode(mask, LOT_STRIP), LOT_STRIP, dilate=True) & mask
    return drop_small(erode(opened, LOT_MARGIN), LOT_MIN_AREA)


def drop_small(mask, min_m2):
    """Without the pockets smaller than min_m2 (a road's widened corner is not a car park)."""
    from collections import deque
    import numpy as np
    keep = np.zeros_like(mask)
    seen = np.zeros_like(mask)
    h, w = mask.shape
    for i0, j0 in zip(*np.nonzero(mask)):
        if seen[i0, j0]:
            continue
        cells, queue = [], deque([(i0, j0)])
        seen[i0, j0] = True
        while queue:
            i, j = queue.popleft()
            cells.append((i, j))
            for a, b in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
                if 0 <= a < h and 0 <= b < w and mask[a, b] and not seen[a, b]:
                    seen[a, b] = True
                    queue.append((a, b))
        if len(cells) * (MASK_RES / 100) ** 2 >= min_m2:
            ii, jj = zip(*cells)
            keep[list(ii), list(jj)] = True
    return keep


def on_asphalt(mask, x, y):
    i, j = int((MASK_X[1] - x) / MASK_RES), int((y - MASK_Y[0]) / MASK_RES)
    return 0 <= i < mask.shape[0] and 0 <= j < mask.shape[1] and bool(mask[i, j])


def car_corners(x, y, yaw, half=CAR_HALF):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for fx, fy in ((0, 0), (half[0], half[1]), (half[0], -half[1]), (-half[0], half[1]), (-half[0], -half[1]),
                   (half[0], 0), (-half[0], 0)):
        yield x + fx * c - fy * s, y + fx * s + fy * c


def angle_to(frm, to):
    return math.degrees(math.atan2(to[1] - frm[1], to[0] - frm[0]))


def angle_diff(a, b):
    return abs((a - b + 180) % 360 - 180)


def lot_exclusions(actors):
    """(blocked(x, y) -> reason or None) for everything the lot must leave clear."""
    targets, boxes, heroes = {}, [], []
    for label, a in actors.items():
        t = call(AT, "get_actor_transform", {"actor": a})
        p = t["location"]
        lx, ly = local_xy(p["x"], p["y"])
        if label.startswith("CT_"):
            targets[label] = (lx, ly, t["rotation"]["yaw"] - SITE_HEADING_DEG)
        elif label.startswith("SI_Car"):
            heroes.append((lx, ly))
        elif label.startswith(("SI_", "LBL_", "TXT_")) and label not in ("SI_ParkingBays", "SI_LotCars", "SI_BowlCrowd"):
            b = call(AT, "get_actor_bounds", {"actor": a})
            pts = [local_xy(x, y) for x in (b["min"]["x"], b["max"]["x"]) for y in (b["min"]["y"], b["max"]["y"])]
            xs, ys = [q[0] for q in pts], [q[1] for q in pts]
            boxes.append((label, min(xs) - 400, max(xs) + 400, min(ys) - 400, max(ys) + 400))
    cam = targets["CT_Parking"][:2]
    sight = [(angle_to(cam, h), math.dist(cam, h)) for h in heroes]
    skip_views = {"CT_StadiumWide", "CT_ExteriorArrival", "CT_Parking"}

    def blocked(x, y):
        if abs(y - ARRIVAL[0]) < 400 and ARRIVAL[1][0] <= x <= ARRIVAL[1][1]:
            return "arrival lane"
        for h in heroes:
            if abs(x - h[0]) < 520 and abs(y - h[1]) < 250:
                return "hero car"
        for label, x0, x1, y0, y1 in boxes:
            if x0 <= x <= x1 and y0 <= y <= y1:
                return label
        for label, (tx, ty, yaw) in targets.items():
            d = math.dist((x, y), (tx, ty))
            if d < 1000 or (label not in skip_views and d < 2500 and angle_diff(angle_to((tx, ty), (x, y)), yaw) < 45):
                return label
        # From the parking camera: nothing between it and a hero car, and nothing close in front of it.
        d = math.dist((x, y), cam)
        a = angle_to(cam, (x, y))
        if d < 1600 and angle_diff(a, targets["CT_Parking"][2]) < 45:
            return "CT_Parking"
        for ha, hd in sight:
            if d < hd and angle_diff(a, ha) < math.degrees(math.atan2(330, hd)) + math.degrees(math.atan2(330, d)):
                return "hero sight line"
        return None
    return blocked


def ism_matrix(x, y, z, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return {"transform": {"xPlane": {"x": c, "y": s, "z": 0, "w": 0}, "yPlane": {"x": -s, "y": c, "z": 0, "w": 0},
                          "zPlane": {"x": 0, "y": 0, "z": 1, "w": 0}, "wPlane": {"x": x, "y": y, "z": z, "w": 1}}}


def refresh(actor):
    """Instances set as properties only reach the renderer when the components re-register: nudge the actor."""
    t = call(AT, "get_actor_transform", {"actor": actor})["location"]
    for dz in (1, 0):
        call(AT, "set_actor_transform", {"actor": actor, "xform": {"location": {"x": t["x"], "y": t["y"], "z": t["z"] + dz}}})


def set_instances(comp, instances):
    """PerInstanceSMData through MCP: the array tool diffs by index, so grow it with identities, then fill it."""
    set_props(comp, {"PerInstanceSMData": []})
    set_props(comp, {"PerInstanceSMData": [ism_matrix(0, 0, 0, 0)] * len(instances)})
    set_props(comp, {"PerInstanceSMData": instances})


def plan_lot(mask, blocked):
    rng = random.Random(SEED)
    mask = parking_mask(mask)
    cands = []
    x0 = HERO_ROW[0]
    n_lo = math.floor((MASK_X[0] - x0) / ROW_PITCH)
    n_hi = math.ceil((MASK_X[1] - x0) / ROW_PITCH)
    reasons = {}
    for n in range(n_lo, n_hi + 1):
        half, bay = (HERO_ROW[1], HERO_ROW[2]) if n == 0 else (ROW_HALF, BAY)
        k_lo, k_hi = math.floor((MASK_Y[0] - BAY0) / bay), math.ceil((MASK_Y[1] - BAY0) / bay)
        for side, yaw in ((-1, 0.0), (1, 180.0)):
            x = x0 + n * ROW_PITCH + side * half
            for k in range(k_lo, k_hi + 1):
                y = BAY0 + k * bay
                if k % CROSS_AISLE[0] in CROSS_AISLE[1]:
                    continue
                if not all(on_asphalt(mask, *p) for p in car_corners(x, y, yaw)):
                    continue
                why = blocked(x, y)
                if why:
                    reasons[why] = reasons.get(why, 0) + 1
                    continue
                if rng.random() < EMPTY_BAYS:
                    continue
                cands.append((x + rng.uniform(-15, 15), y + rng.uniform(-12, 12), yaw + rng.uniform(-1.5, 1.5)))
    print(f"lot: {len(cands)} bays free; left clear: " + ", ".join(f"{k} {v}" for k, v in sorted(reasons.items(), key=lambda kv: -kv[1])[:8]))
    # Ground every car on the (flat) lot; a hit above or below it is something else, so no car there.
    zs = trace_down([world_xy(x, y) for x, y, _ in cands])
    cars = [(x, y, z, yaw) for (x, y, yaw), z in zip(cands, zs) if z is not None and LOT_Z[0] <= z <= LOT_Z[1]]
    rng.shuffle(cars)
    cars = sorted(cars[:MAX_CARS], key=lambda c: (c[0], c[1]))
    print(f"lot: {len(cars)} cars grounded (of {len(cands)} bays)")
    return cars


def step_lot():
    remove_previous("lot")
    remove_previous("interior")
    actors = all_actors()
    mask = build_mask()
    cars = plan_lot(mask, lot_exclusions(actors))

    rng = random.Random(SEED + 1)
    colours = list(PAINT_MIX)
    weights = [PAINT_MIX[c] for c in colours]
    groups = {}  # (model, colour) -> instances
    for x, y, z, yaw in cars:
        model = rng.choice(standins.CITY_CARS)
        colour = rng.choices(colours, weights)[0]
        groups.setdefault((model, colour), []).append(ism_matrix(x, y, z, yaw))

    lot = spawn("SI_LotCars", "lot", "LFF_SiteRoot/LFF_StandIns", tags=(EXTERIOR,))
    n = 0
    by_model = {}
    for (model, colour), inst in sorted(groups.items()):
        by_model.setdefault(model, []).extend(inst)
        base = f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh"
        index, count, paint = standins.car_paint(model, colour)
        overrides = [None] * count
        overrides[index] = paint
        n += 1
        c = call(AT, "add_component", {"owner": lot, "component_type": {"refPath": "/Script/Engine.InstancedStaticMeshComponent"},
                                       "name": f"Body_{model}_{colour}"})
        set_props(c, {"StaticMesh": {"refPath": f"{base}/SM_vehCar_vehicle{model}_No_Wheel.SM_vehCar_vehicle{model}_No_Wheel"},
                      "OverrideMaterials": overrides})
        set_instances(c, inst)
    for model, inst in sorted(by_model.items()):
        base = f"/Game/CitySampleVehicles/vehicle{model}_Car/Mesh"
        parts = [f"SM_Wheel_{w}_vehCar_vehicle{model}" for w in ("Front_L", "Front_R", "Rear_L", "Rear_R")]
        parts.append(f"SM_All_Trans_vehCar_vehicle{model}")
        for part in parts:
            c = call(AT, "add_component", {"owner": lot, "component_type": {"refPath": "/Script/Engine.InstancedStaticMeshComponent"},
                                           "name": part.replace(f"_vehCar_vehicle{model}", f"_{model}").replace("SM_", "")})
            set_props(c, {"StaticMesh": {"refPath": f"{base}/{part}.{part}"}})
            set_instances(c, inst)
            n += 1
    refresh(lot)
    # The ten parked hero cars and the bay lines go with the lot; the arriving car is a cue actor and stays as it is.
    for label, a in actors.items():
        if (label.startswith("SI_Car") and label[6:].isdigit()) or label == "SI_ParkingBays":
            if not call(AT, "has_tag", {"actor": a, "tag": EXTERIOR}):
                call(AT, "add_tag", {"actor": a, "tag": EXTERIOR})
    step_interior()
    print(f"lot: {len(cars)} cars in {n} instanced components on SI_LotCars; {len(by_model)} models; tagged {EXTERIOR}")


# --- interior (the box the journey tests the camera against) -----------------------------------------------------------
# The stadium building from the top-down captures: +-145 m across, -136..+142 m along. The gate cameras
# (CT_InnerGate -159 m, CT_ClubGate -163 m) stay outside it; the seats and the plaza-level concourse are inside.
INTERIOR_CENTRE = (0, 300, 3000)
INTERIOR_EXTENT = (14500, 13900, 5000)


def step_interior():
    remove_previous("interior")
    a = spawn("LFF_StadiumInterior", "interior", "LFF_SiteRoot/LFF_Targets", tags=("LFF_StadiumInterior",))
    b = call(AT, "add_component", {"owner": a, "component_type": {"refPath": "/Script/Engine.BoxComponent"}, "name": "Interior"})
    # Overlaps only (the default profile), so camera sweeps and traces pass through it.
    set_props(b, {"BoxExtent": dict(zip("xyz", INTERIOR_EXTENT)), "RelativeLocation": dict(zip("xyz", INTERIOR_CENTRE)),
                  "bHiddenInGame": True, "bVisible": False, "bCanEverAffectNavigation": False})
    print(f"interior: LFF_StadiumInterior box {INTERIOR_EXTENT} cm half-size at local {INTERIOR_CENTRE}")


# --- ticket ------------------------------------------------------------------------------------------------------------
TICKET_DIR = "/Game/LFF/UI/Ticket"
TX = "editor_toolset.toolsets.texture.TextureTools"


def import_texture(folder, name, source, props):
    """(Re)imports one texture: the MCP importer never overwrites, so the old asset is deleted first."""
    path = f"{folder}/{name}"
    if call(AS, "exists", {"path": path}):
        call(AS, "delete", {"path": path})
    call(TX, "import_file", {"folder_path": folder, "asset_name": name, "source_file": source})
    ref = {"refPath": f"{path}.{name}"}
    set_props(ref, props)
    call(AS, "save_assets", {"asset_paths": [path]})
    return ref


UI_TEXTURE = {"CompressionSettings": "TC_EditorIcon", "LODGroup": "TEXTUREGROUP_UI", "MipGenSettings": "TMGS_NoMipmaps",
              "NeverStream": True, "SRGB": True}


def step_ticket():
    """The phone ticket frames from Tools/Pipeline/prepare_ticket.py (Section 111, Row 10, Seat 8; Eagles vs. Carolina
    Panthers, Sun, October 18), with the same UI settings as Tools/Editor/import_lff_content.py."""
    src = os.path.join(ROOT, "Saved", "TicketImport")
    files = sorted(f for f in os.listdir(src) if f.startswith("T_LFF_Ticket_") and f.endswith(".png"))
    if len(files) != 30:
        raise SystemExit(f"expected 30 ticket frames in {src}; run Tools/Pipeline/prepare_ticket.py")
    for f in files:
        import_texture(TICKET_DIR, f[:-4], os.path.join(src, f), UI_TEXTURE)
    print(f"ticket: {len(files)} frames re-imported to {TICKET_DIR}")


# --- bowl --------------------------------------------------------------------------------------------------------------
CROWD_DIR = "/Game/LFF/Crowd"
MI_TOOLS = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"
MASTER_MASKED = "/Game/LFF/Stadium/Materials/M_LFF_StadiumMasked.M_LFF_StadiumMasked"
# The lower bowl's front wall (traced, local cm): the east and west sidelines, the north end. From the wall the rake
# rises 0.254 m per metre from 1.3 m (profiles across all four sides and the corners; the corners are near square).
BOWL_WALL = {"E": 4100, "W": -4520, "N": -6800, "S": 6200}
RAKE = (130.0, 0.254)                # tread height at 1 m from the wall, rise per unit run (the traces hit treads)
ROW_D = [150 + 85 * n for n in range(5)]   # five rows, 85 cm apart (198 cards, under the cap)
CARD = (800, 100)                    # one row of 16 seats (Tools/Pipeline/prepare_crowd.py)
CARD_SINK = -40                      # the card's bottom edge at seat height, 40 cm above the tread, so torsos and
                                     # heads show over the seat backs of the row in front
MAX_CARDS = 200


def rot_rows(pitch, yaw, roll):
    """FRotationMatrix rows (UE's row-vector convention: world = local * M)."""
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return [(cp * cy, cp * sy, sp), (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp),
            (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)]


def matmul(a, b):
    return [tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3)]


# The engine plane (100 cm, facing +Z) turned to face +X with the texture's width to the viewer's right and its top up
# (as Prop.logo in build_lff_standins.py).
PLANE_TO_CARD = rot_rows(0, -90, 90)


def card_matrix(x, y, z, yaw):
    """A crowd card centred at (x, y, z) facing its +X (yaw) toward the field."""
    s = [(CARD[0] / 100, 0, 0), (0, CARD[1] / 100, 0), (0, 0, 1)]
    m = matmul(matmul(s, PLANE_TO_CARD), rot_rows(0, yaw, 0))
    rows = [dict(zip("xyzw", (*r, 0))) for r in m]
    return {"transform": {"xPlane": rows[0], "yPlane": rows[1], "zPlane": rows[2], "wPlane": {"x": x, "y": y, "z": z, "w": 1}}}


def plan_bowl():
    """(x, y, z, yaw) card centres: five rows of the two sidelines and the north end (the aerial sees the
    north end over the south stand; the south end is behind it, so it's left out)."""
    cards = []
    for n, d in enumerate(ROW_D):
        z = RAKE[0] + RAKE[1] * (d - 100) - CARD_SINK + CARD[1] / 2
        # Alternate rows start half a card along, so the card joins don't line up up the rake.
        shift = (n % 2) * CARD[0] / 2
        for side, yaw in (("E", 180.0), ("W", 0.0)):
            x = BOWL_WALL[side] + (d if side == "E" else -d)
            y0, y1 = BOWL_WALL["N"] + 300, BOWL_WALL["S"] - 300
            k = int((y1 - y0 - shift) // CARD[0])
            start = (y0 + y1) / 2 - k * CARD[0] / 2 + shift / 2
            cards += [(x, start + CARD[0] * (i + 0.5), z, yaw) for i in range(k)]
        y = BOWL_WALL["N"] - d
        x0, x1 = BOWL_WALL["W"] + 300, BOWL_WALL["E"] - 300
        k = int((x1 - x0 - shift) // CARD[0])
        start = (x0 + x1) / 2 - k * CARD[0] / 2 + shift / 2
        cards += [(start + CARD[0] * (i + 0.5), y, z, 90.0) for i in range(k)]
    return cards[:MAX_CARDS]


def step_bowl():
    remove_previous("bowl")
    # The cards are instances: without this usage flag the masked master draws nothing on them (and a cooked build
    # would fall back to the default material).
    master = {"refPath": MASTER_MASKED}
    if not call(OT, "get_properties", {"instance": master, "properties": ["bUsedWithInstancedStaticMeshes"]})["bUsedWithInstancedStaticMeshes"]:
        set_props(master, {"bUsedWithInstancedStaticMeshes": True})
        call(AS, "save_assets", {"asset_paths": [MASTER_MASKED.split(".")[0]]})
    src = os.path.join(ROOT, "Saved", "CrowdImport")
    mis = []
    for v in ("A", "B"):
        path = os.path.join(src, f"T_LFF_CrowdCard_{v}.png")
        if not os.path.exists(path):
            raise SystemExit(f"missing {path}; run Tools/Pipeline/prepare_crowd.py")
        tex = import_texture(CROWD_DIR, f"T_LFF_CrowdCard_{v}", path, {"SRGB": True})
        name = f"MI_LFF_CrowdCard_{v}"
        if not call(AS, "exists", {"path": f"{CROWD_DIR}/{name}"}):
            call(MI_TOOLS, "create", {"folder_path": CROWD_DIR, "asset_name": name, "parent": {"refPath": MASTER_MASKED}})
        mi = {"refPath": f"{CROWD_DIR}/{name}.{name}"}
        call(MI_TOOLS, "set_texture_parameter", {"instance": mi, "name": "BaseColorMap", "value": tex})
        call(MI_TOOLS, "set_scalar_parameter", {"instance": mi, "name": "Roughness", "value": 0.9})
        call(MI_TOOLS, "set_scalar_parameter", {"instance": mi, "name": "Specular", "value": 0.2})
        call(AS, "save_assets", {"asset_paths": [f"{CROWD_DIR}/{name}"]})
        mis.append(mi)

    cards = plan_bowl()
    crowd = spawn("SI_BowlCrowd", "bowl", "LFF_SiteRoot/LFF_Crowd", tags=("LFF_Crowd",))
    for v, mi in zip("AB", mis):
        c = call(AT, "add_component", {"owner": crowd, "component_type": {"refPath": "/Script/Engine.InstancedStaticMeshComponent"},
                                       "name": f"Cards{v}"})
        set_props(c, {"StaticMesh": {"refPath": "/Engine/BasicShapes/Plane.Plane"}, "OverrideMaterials": [mi], "CastShadow": False})
        set_instances(c, [card_matrix(*cd) for i, cd in enumerate(cards) if (i % 2 == 0) == (v == "A")])
    refresh(crowd)
    print(f"bowl: {len(cards)} crowd cards (5 rows: both sidelines and the north end) on SI_BowlCrowd, tagged LFF_Crowd")


STEPS = {"sprites": step_sprites, "parking": step_parking, "people": step_people, "lot": step_lot, "ticket": step_ticket,
         "bowl": step_bowl}


def main(names):
    if call(EA, "IsPIERunning", {}):
        raise SystemExit("Stop PIE first (assets are saved at the end).")
    for name in names or list(STEPS):
        STEPS[name]()
    call(AS, "save_assets", {"asset_paths": [LEVEL]})
    print("level saved")


if __name__ == "__main__":
    main(sys.argv[1:])
