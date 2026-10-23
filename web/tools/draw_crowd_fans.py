"""Writes the bowl crowd's eight billboard fans as one atlas for the web build (8 x 128 x 512 px, looks A-H left to
right). The drawing is Unreal's own: Tools/Editor/build_crowd_billboards.py fan(), same seeds, so both builds show
the same fans. Needs Pillow; called by tools/prepare_assets.mjs.

  python3 web/tools/draw_crowd_fans.py <out.png>
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "Tools", "Editor"))
from PIL import Image  # noqa: E402

import build_crowd_billboards as unreal  # noqa: E402


def main(out):
    atlas = Image.new("RGBA", (unreal.W * unreal.LOOKS, unreal.H), (0, 0, 0, 0))
    for i in range(unreal.LOOKS):
        atlas.paste(unreal.fan(unreal.SEED + 101 * i), (unreal.W * i, 0))
    atlas.save(out)


if __name__ == "__main__":
    main(sys.argv[1])
