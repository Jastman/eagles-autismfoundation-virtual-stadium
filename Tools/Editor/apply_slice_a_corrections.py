"""One-off, in place (no people spawned or rebuilt): deletes the red stand flares, makes the line's front two people
(SI_QueuePerson13 and 12) the security walkers in place of SI_SecurityWalker1/2, rebuilds the bag table with a clear
bag, and adds the green gate flare bursts and CT_GateFlares for step 13. Rerunning build_lff_standins.py or
rebuild_slice_a_people.py gives the same result.
  python3 Tools/Editor/apply_slice_a_corrections.py
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import build_lff_standins as s  # noqa: E402
import finish_pass  # noqa: E402
from unreal_mcp import call  # noqa: E402

DT = "editor_toolset.toolsets.data_table.DataTableTools"
TABLE = {"refPath": "/Game/LFF/Data/DT_LFF_GoldenPath.DT_LFF_GoldenPath"}


def find(label):
    return [a for a in call(s.SC, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []]


def main():
    if call("EditorToolset.EditorAppToolset", "IsPIERunning", {}):
        raise SystemExit("stop PIE first")
    s.SITE_ROOT = find("LFF_SiteRoot")[0]
    s.MATS = {name: s.material(name) for name in s.COLOURS}
    s.MATS["ClearBag"] = s.clear_bag_material()
    gone = []
    for label in ("SI_GateFlare", "SI_SecurityWalker", "SI_BagTable", "CT_GateFlares"):
        for a in find(label):
            gone.append(call(s.AT, "get_label", {"actor": a}) if True else label)
            call(s.SC, "remove_from_scene", {"actor": a})
    print("deleted:", gone)

    gx, gy = s.PLAZA_GATE
    points = json.load(open(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                         "Content", "LFF", "Crowd", "queue.json")))["seats"][::2]
    for num, (end_y, delay) in s.security_walkers(points).items():
        x, y, z, yaw = points[num - 1]
        run = end_y - y
        a = next(a for a in find(f"SI_QueuePerson{num}") if call(s.AT, "get_label", {"actor": a}) == f"SI_QueuePerson{num}")
        call(s.AT, "set_actor_transform", {"actor": a, "worldspace": False, "xform": s.xform((gx, end_y, z), (0, 90, 0))})
        if call(s.AT, "has_tag", {"actor": a, "tag": "LFF_Crowd"}):
            call(s.AT, "remove_tag", {"actor": a, "tag": "LFF_Crowd"})
        s.set_props(a, {"CueId": "SecurityWalk", "Trigger": "Arrival", "PlaySeconds": run / s.WALK_SPEED, "PlayDelaySeconds": delay,
                        "bLinear": True, "bHiddenBefore": False, "bHiddenAfter": False, "StartOffset": {"x": -run, "y": 0, "z": 0},
                        "MovingAnimation": {"refPath": s.MH_WALK}})
        print(f"SI_QueuePerson{num} walks {run:.0f} cm through the middle arch after {delay:.1f} s")

    s.clear_bag_table(gx, gy)
    s.gate_flare_bursts()
    for a in find("CT_GateFlares"):
        finish_pass.hide_icons(a)
    call(DT, "set_rows", {"data_table": TABLE, "values": json.dumps({"gate-flares": {"sceneCues": ["GateFlareBursts"], "cameraTargetId": "CT_GateFlares"}})})
    row = call(DT, "get_rows", {"data_table": TABLE, "row_names": ["gate-flares"]})["gate-flares"]
    print("gate-flares row:", row["sceneCues"], row["cameraTargetId"])
    call(s.AS, "save_assets", {"asset_paths": [s.LEVEL, "/Game/LFF/Data/DT_LFF_GoldenPath"]})
    print("level and table saved")


if __name__ == "__main__":
    main()
