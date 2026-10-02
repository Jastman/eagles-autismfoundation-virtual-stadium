"""Draws the caption card's voice button icons (white on transparent, 128 px): a speaker with sound waves (voice
on) and a speaker crossed out (voice off). Nothing is fetched.

  python3 Tools/Pipeline/prepare_icons.py     # Saved/IconImport/T_LFF_Icon_Voice{On,Off}.png
Import with Tools/Editor/session5_assets.py (UI textures). Needs Pillow.
"""
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Saved", "IconImport")
S, K = 128, 4  # size, supersampling
WHITE = (245, 243, 236, 255)


def speaker(d):
    u = S * K / 128
    d.polygon([(18 * u, 48 * u), (40 * u, 48 * u), (68 * u, 22 * u), (68 * u, 106 * u), (40 * u, 80 * u), (18 * u, 80 * u)], fill=WHITE)


def icon(on):
    img = Image.new("RGBA", (S * K, S * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    u = S * K / 128
    speaker(d)
    w = int(9 * u)
    if on:
        for r in (22, 40):
            d.arc((64 * u - r * u, 64 * u - r * u, 64 * u + r * u, 64 * u + r * u), -48, 48, fill=WHITE, width=w)
    else:
        d.line((84 * u, 46 * u, 116 * u, 82 * u), fill=WHITE, width=w)
        d.line((116 * u, 46 * u, 84 * u, 82 * u), fill=WHITE, width=w)
    return img.resize((S, S), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, on in (("T_LFF_Icon_VoiceOn", True), ("T_LFF_Icon_VoiceOff", False)):
        icon(on).save(os.path.join(OUT, f"{name}.png"))
        print("wrote", name)


if __name__ == "__main__":
    main()
