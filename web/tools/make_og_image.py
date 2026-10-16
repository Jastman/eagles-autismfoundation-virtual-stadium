"""Composes the link-preview image (web/static/og-image.jpg, 1200 x 630) from a render of the stadium: the welcome
screen's teal fade, the Eagles Autism Foundation and Five Below Foundation logos and the GAMEDAY VISUAL SCHEDULE headline,
without the buttons. The render is a screenshot of the page at 1200 x 630 with the HUD hidden (any headless browser).

  python3 web/tools/make_og_image.py <render.png> [web/static/og-image.jpg]
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
UI = os.path.join(HERE, "..", "..", "SourceData", "UI")
SIZE = (1200, 630)
MIDNIGHT = (0, 76, 84)


def font(name, size):
    return ImageFont.truetype(os.path.join(UI, "Fonts", name), size)


def logo(name, height):
    img = Image.open(os.path.join(UI, name)).convert("RGBA")
    return img.resize((round(img.width * height / img.height), height), Image.LANCZOS)


def main(render, out):
    scene = Image.open(render).convert("RGB").resize(SIZE, Image.LANCZOS)
    # The welcome screen's fade: midnight green through T_LFF_WelcomeFade's alpha, stretched to the frame (hud.css).
    fade_alpha = Image.open(os.path.join(UI, "T_LFF_WelcomeFade.png")).convert("RGBA").split()[3].resize(SIZE, Image.LANCZOS)
    scene.paste(Image.new("RGB", SIZE, MIDNIGHT), (0, 0), fade_alpha)
    # A soft left-to-right veil so the headline reads on any frame.
    veil = Image.new("L", SIZE, 0)
    ImageDraw.Draw(veil).rectangle((0, 0, 560, SIZE[1]), fill=110)
    scene.paste(Image.new("RGB", SIZE, MIDNIGHT), (0, 0), veil.filter(ImageFilter.GaussianBlur(140)))

    left, y = 64, 196
    eaf, fivebelow = logo("T_LFF_Logo_EAF_White.png", 66), logo("T_LFF_Logo_FiveBelow_UI.png", 50)
    scene.paste(eaf, (left, y), eaf)
    scene.paste(fivebelow, (left + eaf.width + 40, y + (eaf.height - fivebelow.height) // 2), fivebelow)
    y += eaf.height + 30

    draw = ImageDraw.Draw(scene)
    head = font("AtkinsonHyperlegible-Bold.ttf", 76)
    for line in ("GAMEDAY VISUAL", "SCHEDULE"):
        draw.text((left + 2, y + 3), line, font=head, fill=(0, 0, 0))  # Unreal's text shadow, offset down
        draw.text((left, y), line, font=head, fill=(255, 255, 255))
        y += 74
    y += 18
    draw.text((left, y), "A calm, step-by-step preview of game day", font=font("AtkinsonHyperlegible-Regular.ttf", 28), fill=(232, 240, 240))
    draw.text((left, y + 38), "Lincoln Financial Field", font=font("AtkinsonHyperlegible-Bold.ttf", 28), fill=(105, 190, 40))

    credit = font("AtkinsonHyperlegible-Regular.ttf", 14)
    text = "Imagery © Google · Cesium"
    w = draw.textlength(text, font=credit)
    draw.text((SIZE[0] - w - 16, SIZE[1] - 26), text, font=credit, fill=(235, 235, 235))

    scene.save(out, "JPEG", quality=88, optimize=True, progressive=True)
    print(f"{out}: {os.path.getsize(out) // 1024} KB")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "static", "og-image.jpg"))
