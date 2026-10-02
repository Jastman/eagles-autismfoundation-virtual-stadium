"""Writes the soft backgrounds the HUD text sits on (Docs/UI_DESIGN.md), white with an alpha falloff; UMG tints them.

  SourceData/UI/T_LFF_Fog.png         256 x 256, drawn as a 9-slice Box (margin 0.375 = 96 px): a solid middle that stretches
                                      to fit the text and a 96 px feathered edge with rounded corners. No hard edge anywhere.
  SourceData/UI/T_LFF_FadeTop.png     16 x 256, solid at the top, clear at the bottom (behind the header).
  SourceData/UI/T_LFF_FadeBottom.png  the same, flipped (behind the chapter bar and the credits).
  SourceData/UI/T_LFF_FadeSide.png    256 x 16, clear at the left, solid at the right (the Settings sheet's inner edge).

Run from the repo root: python3 Tools/Pipeline/prepare_ui_fog.py   (then Tools/Editor/import_ui_fog.py in the editor)
"""
import math
import os

from PIL import Image

OUT = os.path.join("SourceData", "UI")
SIZE, MARGIN, FEATHER = 256, 96, 88


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def fog():
    img = Image.new("RGBA", (SIZE, SIZE))
    px = img.load()
    inner_lo, inner_hi = MARGIN, SIZE - 1 - MARGIN
    for y in range(SIZE):
        for x in range(SIZE):
            # Distance outside the solid middle, measured to a rounded rectangle, so the corners fade round too.
            dx = max(inner_lo - x, 0, x - inner_hi)
            dy = max(inner_lo - y, 0, y - inner_hi)
            depth = MARGIN - math.hypot(dx, dy)
            px[x, y] = (255, 255, 255, round(255 * smooth(depth / FEATHER)))
    img.save(os.path.join(OUT, "T_LFF_Fog.png"))


def fade(name, top_solid):
    img = Image.new("RGBA", (16, 256))
    px = img.load()
    for y in range(256):
        t = y / 255.0
        a = smooth(1.0 - t) if top_solid else smooth(t)
        for x in range(16):
            px[x, y] = (255, 255, 255, round(255 * a))
    img.save(os.path.join(OUT, name))


def fade_side():
    img = Image.new("RGBA", (256, 16))
    px = img.load()
    for x in range(256):
        a = smooth(x / 255.0)
        for y in range(16):
            px[x, y] = (255, 255, 255, round(255 * a))
    img.save(os.path.join(OUT, "T_LFF_FadeSide.png"))


if __name__ == "__main__":
    fog()
    fade("T_LFF_FadeTop.png", True)
    fade("T_LFF_FadeBottom.png", False)
    fade_side()
    print("wrote T_LFF_Fog.png, T_LFF_FadeTop.png, T_LFF_FadeBottom.png, T_LFF_FadeSide.png to", OUT)
