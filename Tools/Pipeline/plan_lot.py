"""Plans the parking lot (session 5): bays on the lot's own grid, the cars in them, painted stall lines, the parking
step's hero cars and camera, and the queue at the Pepsi Plaza entry. Writes Saved/LotPlan/plan.json, which
Tools/Editor/session5_assets.py places in the level (and traces onto the ground).

  python3 Tools/Pipeline/plan_lot.py
Needs numpy and Pillow, and the asphalt mask Saved/LotMask/lot_mask.npy (Tools/Editor/finish_pass.py builds it from
top-down captures).

The model's lot is laid out on a grid turned 16.5 degrees from the stadium frame (its walkway islands and the slab's
edges, measured from the mask: two line families at 16.5 and 106.5 degrees). Bays follow it: 2.7 m wide double rows,
noses 80 cm apart, 7.2 m aisles; a cross aisle every 20 bays; strips under 12 m wide are roads, not parking. The parking
camera stands in an aisle looking south down the rows, so the stadium is behind the cars; ten hero cars fill the bays
either side and the arriving car pulls into the empty bay on the right. The Pepsi Plaza entry (line, security, gate,
bag check, ticket) keeps 10 m of clear ground and its cameras' views.
Local coordinates: the stadium frame (LFF_SiteRoot): +X toward the east stand, +Y toward the south end, cm.
"""
import json
import math
import os
import random
import sys

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "Editor"))
import finish_pass as fp  # noqa: E402  (mask helpers only; no editor calls)

GRID_DEG = 16.5
A = (math.cos(math.radians(GRID_DEG)), math.sin(math.radians(GRID_DEG)))     # car length, across the rows
B = (-math.sin(math.radians(GRID_DEG)), math.cos(math.radians(GRID_DEG)))    # along the rows (toward the stadium)
U0, V0 = -3869.0, -23624.0          # the hero double row, where the old hero rows were
ROW_PITCH, ROW_HALF, BAY = 1800, 280, 270
CROSS_AISLE = (20, (10, 11))
EMPTY_BAYS = 0.05
STALL_LEN, LINE_W = 500, 10
SEED = 1018

# Pepsi Plaza entry (build_lff_standins.py PLAZA_GATE and the props around it), with its five step cameras.
GATE = (-1500, -22800)
ENTRY_BOX = (-3400, 700, -26800, -21400)   # x0, x1, y0, y1: the queue, security, gate, bag table, ticket, +10 m
ENTRY_CAMS = {"CT_Line": (-2600, -25500, 53.7), "CT_Security": (-2200, -24300, 50.5), "CT_PlazaGate": (-2800, -23950, 41.5),
              "CT_BagCheck": (-1400, -24250, 50.9), "CT_Ticket": (-1750, -23600, 47.3)}
OTHER_CAMS = {"CT_Plaza": (-9300, -21900, 27.6), "CT_Popcorn": (-3700, -20550, 90.0), "CT_SensoryVehicle": (-7950, -21350, 110.2),
              "CT_ClubGate": (-4650, -16300, 67.8), "CT_InnerGate": (-800, -15900, 61.1), "CT_GateEntry": (-1500, -17000, 66.8)}

# The parking step: camera in the aisle north of the hero row, looking south down it.
CAM_BACK = 900                     # cm before the first hero bay
CAM_EYE, CAM_PITCH = 240, -6.0
HERO_BAYS = 6                      # per side (the ten nearest the camera are used)
ARRIVAL_K = 2                      # the right-hand bay the car rolls into


def to_local(u, v):
    return u * A[0] + v * B[0], u * A[1] + v * B[1]


def to_lot(x, y):
    return x * A[0] + y * A[1], x * B[0] + y * B[1]


def angle_diff(a, b):
    return abs((a - b + 180) % 360 - 180)


def footprint_ok(mask, u, v):
    for du in (-290, 0, 290):
        for dv in (-130, 0, 130):
            if not fp.on_asphalt(mask, *to_local(u + du, v + dv)):
                return False
    return True


def blocked(x, y, cam):
    if ENTRY_BOX[0] <= x <= ENTRY_BOX[1] and ENTRY_BOX[2] <= y <= ENTRY_BOX[3]:
        return "entry"
    for name, (cx, cy, yaw) in {**ENTRY_CAMS, **OTHER_CAMS}.items():
        d = math.hypot(x - cx, y - cy)
        a = math.degrees(math.atan2(y - cy, x - cx))
        if d < 1000 or (d < (3500 if name in ENTRY_CAMS else 2500) and angle_diff(a, yaw) < 45):
            return name
    if math.hypot(x - cam[0], y - cam[1]) < 600:
        return "parking camera"
    return None


# The old hero rows and their bay lines stood here when the lot was captured, so the mask has a hole there.
OLD_HERO_BOX = (2050, 3950, -24900, -22900)


def main():
    raw = np.load(os.path.join(ROOT, "Saved", "LotMask", "lot_mask.npy"))
    x0, x1, y0, y1 = OLD_HERO_BOX
    i0, i1 = int((fp.MASK_X[1] - x1) / fp.MASK_RES), int((fp.MASK_X[1] - x0) / fp.MASK_RES)
    j0, j1 = int((y0 - fp.MASK_Y[0]) / fp.MASK_RES), int((y1 - fp.MASK_Y[0]) / fp.MASK_RES)
    raw[i0:i1 + 1, j0:j1 + 1] = True
    mask = fp.parking_mask(raw)
    rng = random.Random(SEED)
    cam_u, cam_v = U0 + ROW_PITCH / 2, V0 - CAM_BACK
    cam = to_local(cam_u, cam_v)
    left_row, right_row = (0, 1), (1, -1)       # (module, side): the rows either side of the camera's aisle
    hero, arrival, cars, lines, reasons = [], None, [], [], {}

    n_lo, n_hi = -16, 16
    for n in range(n_lo, n_hi + 1):
        uc = U0 + n * ROW_PITCH
        for side in (-1, 1):
            u = uc + side * ROW_HALF
            yaw = GRID_DEG + (0.0 if side < 0 else 180.0)   # noses toward the double row's centre line
            valid = []
            for k in range(-200, 200):
                v = V0 + k * BAY
                if k % CROSS_AISLE[0] in CROSS_AISLE[1]:
                    continue
                if not footprint_ok(mask, u, v):
                    continue
                x, y = to_local(u, v)
                if ENTRY_BOX[0] <= x <= ENTRY_BOX[1] and ENTRY_BOX[2] <= y <= ENTRY_BOX[3]:
                    continue
                valid.append(k)
                is_aisle_row = (n, side) in (left_row, right_row)
                if is_aisle_row and 0 <= k < HERO_BAYS + (1 if (n, side) == right_row else 0):
                    if (n, side) == right_row and k == ARRIVAL_K:
                        arrival = (x, y, yaw)
                    else:
                        hero.append((x, y, yaw))
                    continue
                if is_aisle_row and k < 0 and V0 + k * BAY > cam_v - 300:
                    reasons["beside the camera"] = reasons.get("beside the camera", 0) + 1
                    continue
                why = blocked(x, y, cam)
                if why:
                    reasons[why] = reasons.get(why, 0) + 1
                    continue
                if rng.random() < EMPTY_BAYS:
                    continue
                jx, jy = rng.uniform(-6, 6), rng.uniform(-6, 6)
                cars.append((round(x + jx, 1), round(y + jy, 1), round(yaw + rng.uniform(-0.6, 0.6), 2)))
            # Paint: a stall line between neighbouring bays (and at each end), across the stall; and the double row's
            # centre line under the noses, one piece per bay.
            ks = set(valid)
            done = set()
            for k in sorted(ks):
                for e in (k - 0.5, k + 0.5):
                    if e in done:
                        continue
                    done.add(e)
                    lx, ly = to_local(uc + side * (40 + STALL_LEN / 2), V0 + e * BAY)
                    lines.append((round(lx, 1), round(ly, 1), GRID_DEG, STALL_LEN, LINE_W))
                if side < 0:  # the centre line once per double row
                    cx, cy = to_local(uc, V0 + k * BAY)
                    lines.append((round(cx, 1), round(cy, 1), GRID_DEG + 90, BAY, LINE_W))

    # The ten nearest the camera; the rest of the hero bays get lot cars.
    hero.sort(key=lambda h: math.hypot(h[0] - cam[0], h[1] - cam[1]))
    for x, y, yaw in hero[10:]:
        cars.append((round(x, 1), round(y, 1), yaw))
    plan = {
        "grid_deg": GRID_DEG,
        "cars": cars,
        "hero": hero[:10],
        "arrival": arrival,
        "arrival_start_offset": [-(ROW_PITCH / 2 - ROW_HALF) - 40, 0, 0],
        "lines": lines,
        "camera": {"x": round(cam[0], 1), "y": round(cam[1], 1), "eye": CAM_EYE, "pitch": CAM_PITCH, "yaw": GRID_DEG + 90},
        "sign": {"pos": [round(c, 1) for c in to_local(cam_u, V0 + 2600)], "face": [round(cam[0], 1), round(cam[1], 1)]},
        "queue": queue_points(rng),
    }
    os.makedirs(os.path.join(ROOT, "Saved", "LotPlan"), exist_ok=True)
    with open(os.path.join(ROOT, "Saved", "LotPlan", "plan.json"), "w") as f:
        json.dump(plan, f)
    print(f"{len(cars)} lot cars, {len(plan['hero'])} hero bays + arrival {'yes' if arrival else 'NO'}, {len(lines)} paint "
          f"lines, {len(plan['queue'])} people in the queue; camera at local {plan['camera']['x']}, {plan['camera']['y']}; "
          f"left clear: {dict(sorted(reasons.items(), key=lambda kv: -kv[1]))}")


def queue_points(rng):
    """People waiting in the three stanchion lanes in front of security (build_lff_standins.py: posts at x = gate
    -450/-150/150/450, from 19 m to 8 m before the gate), and a tail beyond them, all facing the gate (+Y)."""
    gx, gy = GATE
    pts = []
    for lane_x in (-300, 0, 300):
        y = gy - 1950
        while y < gy - 900:
            if rng.random() > 0.08:  # a gap here and there
                pts.append([round(gx + lane_x + rng.uniform(-12, 12), 1), round(y + rng.uniform(-10, 10), 1), 0.0,
                            round(90 + rng.uniform(-12, 12), 1)])
            y += rng.uniform(62, 85)
    # The line carries on past the posts, two abreast, bending away from the line camera.
    y, x = gy - 2000, gx + 150
    while y > gy - 2900:
        for dx in (-45, 45):
            pts.append([round(x + dx + rng.uniform(-10, 10), 1), round(y + rng.uniform(-10, 10), 1), 0.0, round(90 + rng.uniform(-15, 15), 1)])
        y -= rng.uniform(65, 85)
        x += 25
    return pts


if __name__ == "__main__":
    main()
