"""One-off, in place: the bright streaks on the plaza-level concourse ceiling (steps 14 and 20-23).
They're thin faces of the model's lighter concrete (slot Concrete3, MI_LFF_Concrete3) lying on the ceiling concrete
(Concrete1) of SM_LFF_StructureS4In: north of about y = 0 they're S4In's own, south of it (food) SM_LFF_StructureS3In's
(hiding chunks and painting slots red one by one turns only the streaks red). So both chunks' Concrete3 slot is
overridden with Concrete1 on their components: the streaks are the same concrete as the ceiling under them. From the
tour's cameras Concrete3 shows nowhere else in S4In; in S3In it's also the low wall in front of the first row (seat
view), which becomes the slightly darker Concrete1. The mesh assets aren't changed. Also removes SI_ConcourseCeilingPlate, the cover
plate tried first (a flat plate can't follow the ceiling where it curves down into the inner wall).
Stop PIE first.
  python3 Tools/Editor/fix_concourse_ceiling.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import finish_pass as fp  # noqa: E402
from unreal_mcp import call  # noqa: E402

CHUNKS = ("SM_LFF_StructureS4In", "SM_LFF_StructureS3In")
SLOT, TO = "Concrete3", "/Game/LFF/Stadium/Materials/MI_LFF_Concrete1.MI_LFF_Concrete1"


def exact(label):
    return [a for a in call(fp.SC, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []
            if call(fp.AT, "get_label", {"actor": a}) == label]


def main():
    if call(fp.EA, "IsPIERunning", {}):
        raise SystemExit("stop PIE first")
    for a in exact("SI_ConcourseCeilingPlate"):
        call(fp.SC, "remove_from_scene", {"actor": a})

    for chunk in CHUNKS:
        mesh = {"refPath": f"/Game/LFF/Stadium/Meshes/{chunk}.{chunk}"}
        slots = [m["materialSlotName"] for m in call(fp.OT, "get_properties", {"instance": mesh, "properties": ["StaticMaterials"]})["StaticMaterials"]]
        comp = next(c for c in call(fp.AT, "get_components", {"actor": exact(chunk)[0]}) if "StaticMeshComponent" in c["refPath"])
        fp.set_props(comp, {"OverrideMaterials": []})  # arrays can't shrink and change at once
        fp.set_props(comp, {"OverrideMaterials": [{"refPath": TO} if name == SLOT else None for name in slots]})
        print(f"{chunk}: slot {SLOT} (#{slots.index(SLOT)} of {len(slots)}) shows MI_LFF_Concrete1")
    call(fp.AS, "save_assets", {"asset_paths": [fp.LEVEL]})
    print("cover plate removed; level saved")


if __name__ == "__main__":
    main()
