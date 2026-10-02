"""Rebuilds only the people and the flares in L_LFF_Stadium: the security checkpoint (step 05: arches, two walkers),
the clear-bag close-up (06), the clothed line at the Pepsi Plaza entry (replacing session 5's Mass queue crowd), the
gate flares (13) and the step 22 staff member. Every other stand-in (gates, cars, eagle, fireworks) is left as it is,
which a full build_lff_standins.py rerun would not do.

Run from the repo root with the editor open on L_LFF_Stadium and the MCP server running:
  python3 Tools/Pipeline/prepare_people_prints.py      # the tee prints, once
  python3 Tools/Editor/rebuild_slice_a_people.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import build_lff_standins as s  # noqa: E402
import finish_pass  # noqa: E402
from unreal_mcp import call  # noqa: E402

LABELS = ("SI_SecurityArches", "SI_SecurityWalker", "LBL_Security", "CT_Security", "CT_BagCheck", "SI_StaffAttendant",
          "SI_QueuePerson", "SI_GateFlare", "CT_GateFlares", "SI_BagTable", "ZZ_TestPerson")


def main():
    s.SITE_ROOT = next(a for a in call(s.SC, "find_actors", {"name": "LFF_SiteRoot", "tag": "LFF_SiteRoot", "collision_channels": []}))
    s.MATS = {name: s.material(name) for name in s.COLOURS}
    s.MATS["ClearBag"] = s.clear_bag_material()
    s.people_materials()
    removed = 0
    for label in LABELS:
        for a in call(s.SC, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []:
            call(s.SC, "remove_from_scene", {"actor": a})
            removed += 1
    # Session 5's Mass queue crowd stood in the bind pose; the clothed line replaces it.
    for a in call(s.SC, "find_actors", {"name": "SI_QueueCrowd", "tag": "", "collision_channels": []}) or []:
        call(s.SC, "remove_from_scene", {"actor": a})
        removed += 1
    print(f"removed {removed} actors")
    gx, gy = s.PLAZA_GATE
    s.security_checkpoint(gx, gy)
    s.clear_bag_table(gx, gy)
    s.bag_check_target(gx, gy)
    s.staff_attendant()
    line = s.queue_people()
    s.gate_flare_bursts()
    for label in ("CT_Security", "CT_BagCheck", "CT_GateFlares"):
        for a in call(s.SC, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []:
            finish_pass.hide_icons(a)
    call(s.AS, "save_assets", {"asset_paths": [s.LEVEL]})
    print(f"placed the arches, 2 walkers, {line} people in line, the gate flare bursts, CT_BagCheck and the staff member; level saved")


if __name__ == "__main__":
    main()
