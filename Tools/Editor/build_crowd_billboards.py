"""The bowl's billboard fans: everyone past the MetaHumans around the visitor's seat (ALFFStadiumCrowdSpawner).
Draws eight standing fans (no photos, nothing fetched), imports them as masked materials on the stadium's masked
master, and puts them on the spawner's BillboardMaterials; the spawner places one per seat at play time. Also removes
SI_BowlCrowd, the older five rows of seated crowd cards (finish_pass.py step "bowl"), which the billboards replace.
Stop PIE first. Needs Pillow.
  python3 Tools/Editor/build_crowd_billboards.py
"""
import os
import random
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import finish_pass as fp  # noqa: E402
from unreal_mcp import call  # noqa: E402

OUT = os.path.join(fp.ROOT, "Saved", "CrowdImport")
W, H = 128, 512          # 60 cm x 240 cm (the spawner's BillboardSize); the fan stands on the bottom edge
CM = W / 60.0            # pixels per cm
LOOKS = 8
SEED = 2026
# sRGB, as prepare_crowd.py: Eagles midnight green most, then white, silver, black.
SHIRTS = [((0, 76, 84), 46), ((236, 236, 232), 20), ((165, 172, 175), 14), ((24, 24, 26), 12), ((0, 58, 64), 8)]
PANTS = [((38, 44, 60), 50), ((24, 24, 26), 25), ((90, 84, 72), 15), ((70, 90, 120), 10)]
SKIN = [(241, 205, 170), (224, 172, 128), (198, 134, 92), (141, 85, 54), (96, 60, 38)]
HAIR = [(34, 26, 20), (70, 48, 30), (120, 90, 60), (190, 160, 110), (60, 60, 60)]


def pick(rng, table):
    return rng.choices([c for c, _ in table], [w for _, w in table])[0]


def fan(seed):
    rng = random.Random(seed)
    s = 4  # drawn at 4x and scaled down for soft edges (the mask cuts at alpha 0.33)
    px = CM * s
    img = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, base = W * s / 2, H * s
    k = rng.uniform(0.92, 1.06)  # height 1.6-1.85 m

    def y(cm):  # height above the tread -> pixel row
        return base - cm * k * px

    shirt, pants, skin = pick(rng, SHIRTS), pick(rng, PANTS), rng.choice(SKIN)
    for lx in (-9, 9):  # legs, 0-85 cm
        d.rounded_rectangle((cx + (lx - 7) * px, y(85), cx + (lx + 7) * px, base), radius=4 * px, fill=(*pants, 255))
    d.rounded_rectangle((cx - 20 * px, y(148), cx + 20 * px, y(80)), radius=10 * px, fill=(*shirt, 255))  # torso
    arms_up = rng.random() < 0.25  # some cheering
    for side in (-1, 1):
        ax = cx + side * 24 * px
        if arms_up and side == 1:
            d.rounded_rectangle((ax - 5 * px, y(205), ax + 5 * px, y(140)), radius=5 * px, fill=(*shirt, 255))
            d.ellipse((ax - 5 * px, y(212), ax + 5 * px, y(202)), fill=(*skin, 255))
        else:
            d.rounded_rectangle((ax - 5 * px, y(146), ax + 5 * px, y(92)), radius=5 * px, fill=(*shirt, 255))
            d.ellipse((ax - 5 * px, y(96), ax + 5 * px, y(86)), fill=(*skin, 255))
    d.rectangle((cx - 5 * px, y(156), cx + 5 * px, y(146)), fill=(*skin, 255))  # neck
    hr = 11 * k * px
    hy = y(167)
    d.ellipse((cx - hr, hy - hr, cx + hr, hy + hr), fill=(*skin, 255))
    top = rng.random()
    if top < 0.22:  # a cap in the shirt colours
        d.chord((cx - hr * 1.05, hy - hr * 1.1, cx + hr * 1.05, hy + hr * 0.5), 180, 360, fill=(*pick(rng, SHIRTS), 255))
    elif top < 0.85:
        d.chord((cx - hr, hy - hr * 1.05, cx + hr, hy + hr * 0.3), 180, 360, fill=(*rng.choice(HAIR), 255))
    return img.resize((W, H), Image.LANCZOS)


def main():
    if call(fp.EA, "IsPIERunning", {}):
        raise SystemExit("Stop PIE first (assets and the level are saved at the end).")
    spawners = [a for a in call(fp.SC, "find_actors", {"name": "", "tag": "LFF_Crowd", "collision_channels": []}) or []
                if "LFFStadiumCrowdSpawner" in a["refPath"]]
    if not spawners:
        raise SystemExit("no ALFFStadiumCrowdSpawner in the level")

    os.makedirs(OUT, exist_ok=True)
    mis = []
    for i in range(LOOKS):
        v = chr(ord("A") + i)
        path = os.path.join(OUT, f"T_LFF_CrowdFan_{v}.png")
        fan(SEED + 101 * i).save(path)
        tex = fp.import_texture(fp.CROWD_DIR, f"T_LFF_CrowdFan_{v}", path, {"SRGB": True})
        name = f"MI_LFF_CrowdFan_{v}"
        if not call(fp.AS, "exists", {"path": f"{fp.CROWD_DIR}/{name}"}):
            call(fp.MI_TOOLS, "create", {"folder_path": fp.CROWD_DIR, "asset_name": name, "parent": {"refPath": fp.MASTER_MASKED}})
        mi = {"refPath": f"{fp.CROWD_DIR}/{name}.{name}"}
        call(fp.MI_TOOLS, "set_texture_parameter", {"instance": mi, "name": "BaseColorMap", "value": tex})
        call(fp.MI_TOOLS, "set_scalar_parameter", {"instance": mi, "name": "Roughness", "value": 0.9})
        call(fp.MI_TOOLS, "set_scalar_parameter", {"instance": mi, "name": "Specular", "value": 0.2})
        call(fp.AS, "save_assets", {"asset_paths": [f"{fp.CROWD_DIR}/{name}"]})
        mis.append(mi)

    for spawner in spawners:
        fp.set_props(spawner, {"BillboardMaterials": []})  # arrays can't shrink and change at once
        fp.set_props(spawner, {"BillboardMaterials": mis})

    removed = 0
    for a in call(fp.SC, "find_actors", {"name": "SI_BowlCrowd", "tag": "", "collision_channels": []}) or []:
        if call(fp.AT, "get_label", {"actor": a}) == "SI_BowlCrowd":
            call(fp.SC, "remove_from_scene", {"actor": a})
            removed += 1

    call(fp.AS, "save_assets", {"asset_paths": [fp.LEVEL]})
    print(f"{len(mis)} billboard fans on {len(spawners)} crowd spawner(s); removed {removed} old SI_BowlCrowd; level saved")


if __name__ == "__main__":
    main()
