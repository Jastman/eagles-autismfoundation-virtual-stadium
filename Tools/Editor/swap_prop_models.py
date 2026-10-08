"""Swaps five primitive stand-ins in L_LFF_Stadium for the imported prop models (build_lff_standins.MODELS):
the queue's stanchion lines (step 04) for barriers, the security arches (05) for metal detectors, the clear bag (06),
the sensory trailer (11; its old TXT_SAV_ lettering goes, the model's wrap carries it, and CT_SensoryVehicle moves in
for the real-size trailer) and the food counter (22) for the concession stand, with LBL_Food moved up over it. Each
actor keeps its label and place; nothing else in the level is touched, which a full build_lff_standins.py rerun
would not do. Safe to rerun.

Run from the repo root with the editor open on L_LFF_Stadium and the MCP server running:
  (in the editor) Tools/Editor/import_lff_props.py    # the models, once
  python3 Tools/Editor/swap_prop_models.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import build_lff_standins as s  # noqa: E402
import finish_pass  # noqa: E402
from unreal_mcp import call  # noqa: E402

LABELS = ("SI_QueueStanchions", "SI_SecurityArches", "SI_BagTable", "SI_SensoryTrailer", "TXT_SAV_", "SI_FoodCounter",
          "CT_SensoryVehicle")


def main():
    s.SITE_ROOT = next(a for a in call(s.SC, "find_actors", {"name": "LFF_SiteRoot", "tag": "LFF_SiteRoot", "collision_channels": []}))
    s.MATS = {name: s.material(name) for name in s.COLOURS}
    s.MATS["ClearBag"] = s.clear_bag_material()
    removed = 0
    for label in LABELS:
        for a in call(s.SC, "find_actors", {"name": label, "tag": s.TAG, "collision_channels": []}) or []:
            call(s.SC, "remove_from_scene", {"actor": a})
            removed += 1
    print(f"removed {removed} actors")
    gx, gy = s.PLAZA_GATE
    s.queue_barriers(gx, gy)
    s.security_arches(gx, gy)
    s.clear_bag_table(gx, gy)
    s.sensory_trailer(*s.SAV_AT, face=s.SAV_FACE)
    s.sensory_vehicle_target()  # closer than before: the trailer is at its real size now
    for a in call(s.SC, "find_actors", {"name": "CT_SensoryVehicle", "tag": "", "collision_channels": []}) or []:
        finish_pass.hide_icons(a)
    s.food_stand()
    s.food_sign()
    call(s.AS, "save_assets", {"asset_paths": [s.LEVEL]})
    print("placed the barriers, metal detectors, clear bag, sensory trailer and concession stand; level saved")


if __name__ == "__main__":
    main()
