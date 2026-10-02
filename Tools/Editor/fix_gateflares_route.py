"""One-off, in place: CP_GateFlaresToConcourse's route at eye level (1.6 m over the walk surface): across the plaza
to the foot of the west entry stairs, up them, and a few steps past the top, then a fade and cut into the concourse
(bCutAtEnd). The stairs come from the two marker actors "Stairs Bottom" and "Stairs Top", placed on the steps by hand:
the stadium model has no collision, so traces only find the flat plaza (1.58 m) under them. Move the markers and run
it again to change the route. Stop PIE first.
  python3 Tools/Editor/fix_gateflares_route.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import build_lff_standins as s  # noqa: E402
from unreal_mcp import call  # noqa: E402

LABEL = "CP_GateFlaresToConcourse"
EYE = 160
PLAZA_Z = 158             # plaza walk surface (world cm)
PLAZA = (-8450, -16000)   # world: south of the stairs, clear of the club gate, popcorn cart and sensory trailer
PAST_TOP = 300            # cm on past the top step before the fade


def find_exact(label):
    """The actor labelled `label`, or "<class> - label" (the editor's label for an actor dropped in by hand)."""
    for a in call(s.SC, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []:
        found = call(s.AT, "get_label", {"actor": a})
        if found == label or found.endswith(f" - {label}"):
            return a
    raise SystemExit(f"'{label}' not found")


def marker(label):
    loc = call(s.AT, "get_actor_transform", {"actor": find_exact(label)})["location"]
    return loc["x"], loc["y"], loc["z"]


def main():
    if call("EditorToolset.EditorAppToolset", "IsPIERunning", {}):
        raise SystemExit("stop PIE first")
    path = find_exact(LABEL)
    bottom, top = marker("Stairs Bottom"), marker("Stairs Top")
    dx, dy = top[0] - bottom[0], top[1] - bottom[1]
    run = (dx * dx + dy * dy) ** 0.5
    past = (top[0] + dx / run * PAST_TOP, top[1] + dy / run * PAST_TOP, top[2])
    route = [(*PLAZA, PLAZA_Z), bottom, top, past]
    waypoints = [{"x": x, "y": y, "z": z + EYE} for x, y, z in route]

    # Arrays can't shrink and change at once: clear first, then set the new points.
    s.set_props(path, {"Waypoints": []})
    s.set_props(path, {"Waypoints": waypoints, "bCutAtEnd": True})
    for name, w in zip(("plaza", "stairs foot", "stairs top", "past the top"), waypoints):
        print(f"  {name}: ({w['x']:.0f}, {w['y']:.0f}, {w['z']:.0f})")
    call(s.AS, "save_assets", {"asset_paths": [s.LEVEL]})
    print("level saved")


if __name__ == "__main__":
    main()
