"""Turns the project's logo files (Assets/Logos only; nothing is drawn or fetched) into textures for signs.

  Assets/Logos/Philadelphia-Eagles-Logo.png          ->  Saved/LogoImport/T_LFF_Logo_Eagles.png  (midfield decal)
  Assets/Logos/Eagles-Autism-Foundation logo.png     ->  Saved/LogoImport/T_LFF_Logo_EAF.png     (popcorn cart header)
  Assets/Logos/Wawa-Logo.png                         ->  Saved/LogoImport/T_LFF_Logo_Wawa.png    (popcorn cart header)

Each logo is scaled to fit a power-of-two canvas and centred on transparent padding, so the texture gets mips (the
cart is seen from about 10 m, midfield from 50 m). The logo's own aspect, and how much of the canvas it fills, go to logos.json (the
sizes in Tools/Editor/build_lff_standins.py follow them).

  python3 Tools/Pipeline/prepare_logos.py
Import with Tools/Editor/import_lff_content.py (step "logos"). Needs Pillow.
"""
import json
import os

from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
SRC = os.path.join(ROOT, "Assets", "Logos")
OUT = os.path.join(ROOT, "Saved", "LogoImport")
LOGOS = (("T_LFF_Logo_Eagles", "Philadelphia-Eagles-Logo.png", (2048, 1024)),
         ("T_LFF_Logo_EAF", "Eagles-Autism-Foundation logo.png", (1024, 256)),
         ("T_LFF_Logo_Wawa", "Wawa-Logo.png", (1024, 512)))


def main():
    os.makedirs(OUT, exist_ok=True)
    info = {}
    for name, src, (cw, ch) in LOGOS:
        img = Image.open(os.path.join(SRC, src)).convert("RGBA")
        img = img.crop(img.getchannel("A").getbbox())  # trim any empty margin in the source
        scale = min(cw / img.width, ch / img.height)
        w, h = round(img.width * scale), round(img.height * scale)
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.alpha_composite(img.resize((w, h), Image.LANCZOS), ((cw - w) // 2, (ch - h) // 2))
        canvas.save(os.path.join(OUT, f"{name}.png"))
        info[name] = {"source": f"Assets/Logos/{src}", "canvas": [cw, ch], "logo": [w, h], "aspect": round(w / h, 4)}
        print(f"{name}: {src} -> {cw}x{ch} (logo {w}x{h})")
    with open(os.path.join(OUT, "logos.json"), "w") as f:
        json.dump(info, f, indent=1)


if __name__ == "__main__":
    main()
