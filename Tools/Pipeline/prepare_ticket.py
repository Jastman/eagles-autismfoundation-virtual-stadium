"""Turns Assets/EaglesMobileTicket.gif into UI frames for the rising phone on the ticket step.

The GIF is a phone on a white page with a slow barcode sweep (60 frames, 20 fps). This crops to the phone, makes
the white page transparent (flood fill from the corners, so white inside the screen stays), keeps every other frame
(30 frames, 10 fps: the sweep still reads, at half the texture memory) and scales to 640 px tall.

The GIF's ticket reads SEC 107 / ROW 18 / SEAT 22; the journey's example ticket is Section 111, Row 10, Seat 8
(LBL_Section in Tools/Editor/build_lff_stadium_level.py). No matching still exists, so each frame has the old
numbers filled in from the surrounding background and the example's numbers set in their place, same centre,
same cap height, white on the same grey. The layout doesn't move between frames (only the barcode sweeps).

The GIF's header also reads "Eagles vs. Lions"; the example game is the Carolina Panthers on Sunday, October 18,
2026, 1:00 PM (SourceData/Journey/LFF_GoldenPath.json). The header bar is one flat colour, so the old title is
painted over and set again as two lines: the matchup at the original size and weight, the date under it, smaller.

  python3 Tools/Pipeline/prepare_ticket.py        # writes Saved/TicketImport/T_LFF_Ticket_00.png ...
Import with Tools/Editor/import_lff_content.py. Needs Pillow and numpy.
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
SRC = os.path.join(ROOT, "Assets", "EaglesMobileTicket.gif")
OUT = os.path.join(ROOT, "Saved", "TicketImport")
HEIGHT = 640
KEEP_EVERY = 2

# The seat numbers in the GIF, in source pixels: (left, top, right, bottom) of the white digits, and the example's
# numbers that replace them.
SEAT_NUMBERS = (((241, 392, 273, 407), "111"),   # SEC 107
                ((354, 392, 373, 407), "10"),    # ROW 18
                ((460, 392, 481, 407), "8"))     # SEAT 22
FONT = ("/System/Library/Fonts/Avenir Next.ttc", 2)  # Demi Bold, the nearest system face to the ticket's
FONT_MEDIUM = 5  # Avenir Next Medium, for the date line
# The header bar (flat colour) between the close icon and the right edge, the old title's bounds, and the new lines.
HEADER_BAR = (212, 132, 552, 191)
HEADER_COLOUR = (33, 36, 45)
HEADER_TITLE_BOX = (299, 163, 429, 179)  # "Eagles vs. Lions", ascender to descender
HEADER_TITLE = "Eagles vs. Carolina Panthers"
HEADER_DATE = "Sun, October 18 \u00b7 1:00 PM"


def fit_font(index, sample, height):
    """The largest size of the face whose ink for sample is no taller than height px."""
    size = 8
    while True:
        font = ImageFont.truetype(FONT[0], size + 1, index=index)
        x0, y0, x1, y1 = font.getbbox(sample)
        if y1 - y0 > height:
            return ImageFont.truetype(FONT[0], size, index=index)
        size += 1


def retype_header(out):
    """Replaces "Eagles vs. Lions" with the example game's matchup and date, centred on the phone screen."""
    draw = ImageDraw.Draw(out)
    draw.rectangle(HEADER_BAR, fill=HEADER_COLOUR)
    l, t, r, b = HEADER_TITLE_BOX
    cx = (l + r + 1) / 2
    title = fit_font(FONT[1], "Eagles vs. Lions", b - t + 1)
    date = fit_font(FONT_MEDIUM, "Sun, October 18", round((b - t + 1) * 0.78))
    tx0, ty0, tx1, ty1 = draw.textbbox((0, 0), HEADER_TITLE, font=title)
    dx0, dy0, dx1, dy1 = draw.textbbox((0, 0), HEADER_DATE, font=date)
    gap = 4
    top = (HEADER_BAR[1] + HEADER_BAR[3] + 1) / 2 - ((ty1 - ty0) + gap + (dy1 - dy0)) / 2
    draw.text((round(cx - (tx0 + tx1) / 2), round(top - ty0)), HEADER_TITLE, font=title, fill=(255, 255, 255))
    draw.text((round(cx - (dx0 + dx1) / 2), round(top + (ty1 - ty0) + gap - dy0)), HEADER_DATE, font=date,
              fill=(196, 200, 206))


def retype_seat(frame):
    """Replaces the GIF's section / row / seat with the example ticket's, on one RGBA frame at source size."""
    rgb = np.asarray(frame.convert("RGB"), dtype=np.float32).copy()
    for (l, t, r, b), _ in SEAT_NUMBERS:
        # Fill the old digits (bright pixels, grown by 2 px for the anti-aliased edge) with a harmonic fill from
        # the pixels around them, so the facemask bars behind 107 and 22 carry through instead of leaving a patch.
        l, t, r, b = l - 4, t - 4, r + 5, b + 5
        box = rgb[t:b, l:r]
        text = box.min(axis=2) > 150
        grown = text.copy()
        for dy in (-2, -1, 0, 1, 2):
            for dx in (-2, -1, 0, 1, 2):
                grown |= np.roll(np.roll(text, dy, 0), dx, 1)
        grown[[0, -1], :] = False
        grown[:, [0, -1]] = False
        box[grown] = box[~grown].mean(axis=0)
        for _ in range(400):
            avg = (np.roll(box, 1, 0) + np.roll(box, -1, 0) + np.roll(box, 1, 1) + np.roll(box, -1, 1)) / 4
            box[grown] = avg[grown]
    out = Image.fromarray(rgb.clip(0, 255).astype(np.uint8))
    retype_header(out)
    draw = ImageDraw.Draw(out)
    for (l, t, r, b), number in SEAT_NUMBERS:
        # Size the face so a digit is as tall as the originals (the ticket's digits are 16 px tall at source).
        size = 10
        while True:
            font = ImageFont.truetype(FONT[0], size + 1, index=FONT[1])
            x0, y0, x1, y1 = font.getbbox("0")
            if y1 - y0 > b - t + 1:
                break
            size += 1
        font = ImageFont.truetype(FONT[0], size, index=FONT[1])
        x0, y0, x1, y1 = draw.textbbox((0, 0), number, font=font)
        cx = (l + r + 1) / 2
        draw.text((round(cx - (x0 + x1) / 2), b + 1 - y1), number, font=font, fill=(255, 255, 255))
    out.putalpha(frame.getchannel("A"))
    return out


def frames(gif):
    i = 0
    while True:
        try:
            gif.seek(i)
        except EOFError:
            return
        yield gif.convert("RGBA")
        i += 1


def main():
    os.makedirs(OUT, exist_ok=True)
    gif = Image.open(SRC)
    all_frames = list(frames(gif))
    # The phone's bounds: everything that isn't the white page, taken over the first frame.
    first = all_frames[0].convert("L")
    box = first.point(lambda v: 255 if v < 235 else 0).getbbox()
    kept = all_frames[::KEEP_EVERY]
    for n, frame in enumerate(kept):
        img = retype_seat(frame).crop(box)
        # Page white -> transparent, from each corner only (the rounded corners outside the phone body).
        mask = Image.new("L", img.size, 0)
        probe = img.convert("RGB")
        for corner in ((0, 0), (img.width - 1, 0), (0, img.height - 1), (img.width - 1, img.height - 1)):
            ImageDraw.floodfill(probe, corner, (255, 0, 255), thresh=40)
        for y in range(img.height):
            for x in range(img.width):
                if probe.getpixel((x, y)) == (255, 0, 255):
                    mask.putpixel((x, y), 255)
        alpha = Image.eval(mask, lambda v: 255 - v)
        img.putalpha(alpha)
        w = round(img.width * HEIGHT / img.height)
        img = img.resize((w, HEIGHT), Image.LANCZOS)
        img.save(os.path.join(OUT, f"T_LFF_Ticket_{n:02d}.png"))
    print(f"wrote {len(kept)} frames {w}x{HEIGHT} to {os.path.normpath(OUT)} (crop {box}, "
          f"{1000 * KEEP_EVERY * gif.info.get('duration', 50) / 1000:.0f} ms per frame)")


if __name__ == "__main__":
    main()
