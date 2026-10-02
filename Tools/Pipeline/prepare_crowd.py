"""Paints the bowl crowd card: one row of 16 seated fans seen from the field, for the thin lower-bowl band
(Tools/Editor/finish_pass.py, step "bowl"). Nothing is photographed or fetched; it's drawn here.

A card is 8 m x 1 m of one row (16 seats of 50 cm): each fan is a torso in a shirt colour (midnight green most, then
white, silver, black; no numbers or logos) with a head above it (a range of skin tones, some hair, some caps), with a
little jitter in size and place so the row doesn't read as a grid. Between fans the card is transparent (masked), and
some seats are empty. The cards stand upright on the first rows, so each row's heads show over the row in front, as in
a real crowd. From the aerial and kickoff cameras a fan is 2-6 px: the card has to read as crowd texture, not as a
person up close.

  python3 Tools/Pipeline/prepare_crowd.py     # writes Saved/CrowdImport/T_LFF_CrowdCard_{A,B}.png (1024 x 128)
Needs Pillow.
"""
import os
import random

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
OUT = os.path.join(ROOT, "Saved", "CrowdImport")
W, H = 1024, 128
CARD_CM = (800, 100)
SEATS = 16
EMPTY = 0.12
# sRGB. Eagles midnight green, white, silver, black; mostly green, as on a home game day.
SHIRTS = [((0, 76, 84), 46), ((236, 236, 232), 20), ((165, 172, 175), 14), ((24, 24, 26), 12), ((0, 58, 64), 8)]
SKIN = [(241, 205, 170), (224, 172, 128), (198, 134, 92), (141, 85, 54), (96, 60, 38)]
HAIR = [(34, 26, 20), (70, 48, 30), (120, 90, 60), (190, 160, 110), (60, 60, 60)]


def pick(rng, table):
    return rng.choices([c for c, _ in table], [w for _, w in table])[0]


def card(seed):
    rng = random.Random(seed)
    # Drawn at 4x and scaled down, so the edges are soft (the mask cuts at alpha 0.33).
    s = 4
    px = W * s / CARD_CM[0]  # pixels per cm (the card is 1.28 px/cm at 1x, the same across and up)
    img = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    seat = CARD_CM[0] / SEATS
    base = H * s  # the bottom edge of the card is the seat
    for k in range(SEATS):
        if rng.random() < EMPTY:
            continue
        cx = ((k + 0.5) * seat + rng.uniform(-6, 6)) * px
        scale = rng.uniform(0.9, 1.08)
        tw, th = 40 * scale * px, 56 * scale * px  # shoulders 40 cm wide, seat to shoulder 56 cm
        top = base - th
        shirt = pick(rng, SHIRTS)
        d.rounded_rectangle((cx - tw / 2, top, cx + tw / 2, base + 20), radius=tw * 0.3, fill=(*shirt, 255))
        hr = 11 * scale * px  # a 22 cm head
        hy = top - hr * 0.75
        d.ellipse((cx - hr, hy - hr, cx + hr, hy + hr), fill=(*rng.choice(SKIN), 255))
        cap = rng.random()
        if cap < 0.18:  # a cap in the shirt colours
            d.chord((cx - hr * 1.05, hy - hr * 1.1, cx + hr * 1.05, hy + hr * 0.5), 180, 360, fill=(*pick(rng, SHIRTS), 255))
        elif cap < 0.8:
            d.chord((cx - hr, hy - hr, cx + hr, hy + hr * 0.2), 180, 360, fill=(*rng.choice(HAIR), 255))
    return img.resize((W, H), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, seed in (("A", 18), ("B", 1018)):
        path = os.path.join(OUT, f"T_LFF_CrowdCard_{name}.png")
        card(seed).save(path)
        print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()
