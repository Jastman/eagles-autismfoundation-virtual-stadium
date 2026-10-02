"""One-off, in place: every remaining primitive stand-in person becomes a MetaHuman (ALFFPersonActor, as the queue),
staff are added where the visitor meets them, and the staff step's camera moves in close.
  - Visitors: SI_CrowdLine1-6 (the line), SI_CrowdPlaza1-4, SI_CrowdPopcorn1-3, SI_CrowdSAV1-2 (the sensory trailer).
    The plaza ones were hidden in game (finish_pass.py step "people"); the MetaHumans are shown. Tagged LFF_Crowd.
  - Staff, in black STAFF tees, always shown: SI_TicketStaff (at the gate), SI_SecurityStaff1/2 (beside the outer
    security arches, facing the line), SI_SensoryStaff (inside the sensory room). SI_StaffAttendant (concourse)
    is already a MetaHuman; it is moved back under its sign.
  - CT_Staff: 1.5 m from SI_StaffAttendant at eye level, on the face (step "staff").
The same labels are removed first, so it can run again. Nothing else is touched. Stop PIE first.
  python3 Tools/Editor/convert_people_to_metahumans.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import build_lff_standins as s  # noqa: E402
from unreal_mcp import call  # noqa: E402

TEES = ("Green", "Green", "White", "Black", "Green", "White")


def people():
    """(label, x, y, yaw, tee, crowd[, z]): site-local, as build_lff_standins.py placed the stand-ins."""
    gx, gy = s.PLAZA_GATE
    out = []
    for i, (qx, qy) in enumerate(((-300, -1700), (-300, -1450), (0, -1600), (0, -1300), (300, -1750), (300, -1500))):
        out.append((f"SI_CrowdLine{i + 1}", gx + qx, gy + qy, 90, TEES[i], True))
    for i, (px, py, yaw) in enumerate(((-5200, -20600, 30), (-6000, -19800, 200), (-4600, -21200, 120), (-6800, -21000, 300))):
        out.append((f"SI_CrowdPlaza{i + 1}", px, py, yaw, TEES[i + 1], True))
    pop_at = (-3700, -19600)
    for i, (px, py) in enumerate(((-4180, -20000), (-3230, -20050), (-3330, -19900))):
        out.append((f"SI_CrowdPopcorn{i + 1}", px, py, s.look((px, py, 0), (*pop_at, 0))[1], TEES[i + 2], True))
    sav_at = (-8300, -20400)
    for i, (px, py) in enumerate(((-7500, -20750), (-8950, -20950))):
        out.append((f"SI_CrowdSAV{i + 1}", px, py, s.look((px, py, 0), (*sav_at, 0))[1], TEES[i + 3], True))

    out.append(("SI_TicketStaff", gx + 350, gy - 150, 270, "Staff", False))
    # The arches are at gy - 650, lanes at gx and gx +/- 300 (posts 55 cm either side): just outside the outer posts,
    # clear of the middle lane the two walkers use, facing back down the line.
    for i, side in enumerate((-1, 1)):
        out.append((f"SI_SecurityStaff{i + 1}", gx + side * 470, gy - 600, -90, "Staff", False))
    # Inside the sensory room (SI_SensoryRoomProxy at (-7880, -4820), its frame the site's): left of CT_SensoryRoom's
    # view, clear of the bean bags, facing the camera.
    at, room_cam = (-8030, -4800), (-7880, -4500)
    out.append(("SI_SensoryStaff", *at, s.look((*at, 0), (*room_cam, 0))[1], "Staff", False, SENSORY_FLOOR_Z))
    return out


SENSORY_FLOOR_Z = 166  # the room's floor slab top (world cm)


def remove_exact(label):
    for a in call(s.SC, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []:
        if call(s.AT, "get_label", {"actor": a}) == label:
            call(s.SC, "remove_from_scene", {"actor": a})


def main():
    if call("EditorToolset.EditorAppToolset", "IsPIERunning", {}):
        raise SystemExit("stop PIE first")
    s.SITE_ROOT = call(s.SC, "find_actors", {"name": "LFF_SiteRoot", "tag": "", "collision_channels": []})[0]
    s.people_materials()

    for n, (label, x, y, yaw, tee, crowd, *z) in enumerate(people()):
        remove_exact(label)
        s.metahuman(label, x, y, yaw, n, tee=tee, z=z[0] if z else None, folder=s.FOLDER_CROWD if crowd else s.FOLDER_PROPS,
                    tags=("LFF_Crowd",) if crowd else ())
        print(f"  {label}: MetaHuman, {tee} tee{'' if crowd else ' (staff)'}")

    # The concourse attendant had drifted 8.7 m from its sign: back under LBL_Staff, facing the staff camera.
    sx, sy = s.STAFF_AT
    cx, cy = s.STAFF_CAM
    attendant = next(a for a in call(s.SC, "find_actors", {"name": "SI_StaffAttendant", "tag": "", "collision_channels": []})
                     if call(s.AT, "get_label", {"actor": a}) == "SI_StaffAttendant")
    # In world space (whatever it's attached to now): the site frame turned by the site heading.
    wx, wy = s.world_xy(sx, sy)
    yaw = s.look((sx, sy, 0), (cx, cy, 0))[1] + s.SITE_HEADING_DEG
    call(s.AT, "set_actor_transform", {"actor": attendant, "worldspace": True,
                                       "xform": s.xform((wx, wy, s.ground(sx, sy)), (0, yaw, 0))})
    print("  SI_StaffAttendant: under LBL_Staff, facing CT_Staff")

    # The staff step: in close on the attendant's face, a calm, level view (no looking up or down at them).
    d = math.hypot(cx - sx, cy - sy)
    cam = (sx + (cx - sx) / d * 150, sy + (cy - sy) / d * 150)
    remove_exact("CT_Staff")
    s.target("CT_Staff", cam, (sx, sy, 158), eye=160, fov=40.0)
    print("  CT_Staff: 1.5 m from SI_StaffAttendant, eye level, 40 deg")

    call(s.AS, "save_assets", {"asset_paths": [s.LEVEL]})
    print("level saved")


if __name__ == "__main__":
    main()
