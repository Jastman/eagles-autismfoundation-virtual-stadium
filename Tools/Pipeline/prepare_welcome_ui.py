"""Prepare welcome logos, corner fade and original illustrated Calm Mode toggle scenes."""
import os

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "SourceData", "UI")


def smooth(value):
    value = min(max(value, 0.0), 1.0)
    return value * value * (3.0 - 2.0 * value)


def toggle_scene(calm):
    scale = 3
    width, height = 268, 96
    image = Image.new("RGBA", (width * scale, height * scale))
    top, bottom = ((13, 47, 62), (40, 91, 98)) if calm else ((84, 147, 153), (26, 97, 107))
    draw = ImageDraw.Draw(image)
    for y in range(height * scale):
        mix = y / (height * scale - 1)
        color = tuple(round(a + (b - a) * mix) for a, b in zip(top, bottom))
        draw.line((0, y, width * scale, y), fill=color)

    def ellipse(box, color):
        draw.ellipse(tuple(round(v * scale) for v in box), fill=color)

    def line(points, color, stroke=2):
        draw.line([(round(x * scale), round(y * scale)) for x, y in points],
                  fill=color, width=round(stroke * scale), joint="curve")

    def rectangle(box, color, radius=3):
        draw.rounded_rectangle(tuple(round(v * scale) for v in box),
                               radius=round(radius * scale), fill=color)

    for y in (34, 66):
        line([(18, y + 19), (250, y + 19)], "#477A7E", 1)
        for x in range(25, 247, 28):
            rectangle((x - 9, y - 11, x + 9, y + 3), "#8FBDB5" if calm else "#3B7376")
            rectangle((x - 10, y + 6, x + 10, y + 11), "#B4D2C5" if calm else "#568E87", 2)
            line([(x - 6, y + 12), (x - 6, y + 17)], "#709E99", 1.5)
            line([(x + 6, y + 12), (x + 6, y + 17)], "#709E99", 1.5)

    if not calm:
        skins = ("#EAC6A4", "#B88461", "#805841", "#D9A478")
        shirts = ("#E6EEE6", "#0F4E53", "#A8CFB1", "#2B735F")
        for row, (y, positions) in enumerate(((22, (119, 157, 195, 233)), (56, (105, 143, 181, 219)))):
            for index, x in enumerate(positions):
                skin = skins[(index + row) % len(skins)]
                shirt = shirts[(index + row) % len(shirts)]
                line([(x - 6, y + 10), (x - 13, y + 2), (x - 16, y - 7)], shirt, 4)
                line([(x + 6, y + 10), (x + 13, y + 2), (x + 16, y - 7)], shirt, 4)
                ellipse((x - 18, y - 10, x - 14, y - 6), skin)
                ellipse((x + 14, y - 10, x + 18, y - 6), skin)
                rectangle((x - 8, y + 7, x + 8, y + 25), shirt, 4)
                ellipse((x - 5, y - 4, x + 5, y + 6), skin)
                line([(x - 4, y - 4), (x + 3, y - 4)], "#213E40", 2)
                line([(x, y + 12), (x, y + 16)], "#83B7AD", 1.5)
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, image.width - 1, image.height - 1),
                                           radius=height * scale // 2, fill=255)
    image.putalpha(mask)
    return image


def main():
    os.makedirs(OUT, exist_ok=True)
    source = os.path.join(REPO, "Assets", "Logos", "FiveBelowFoundation.png")
    with Image.open(source) as image:
        logo = image.convert("RGBA")
        bounds = logo.getchannel("A").getbbox()
        if bounds is None:
            raise ValueError(f"Logo is empty: {source}")
        logo.crop(bounds).save(os.path.join(OUT, "T_LFF_Logo_FiveBelow_UI.png"))

    size = 512
    fade = Image.new("RGBA", (size, size))
    fade.putdata([
        (255, 255, 255, round(255 * (1 - smooth((x / (size - 1) - 0.28) / 0.66))
                               * smooth((y / (size - 1) - 0.20) / 0.53)))
        for y in range(size) for x in range(size)
    ])
    fade.save(os.path.join(OUT, "T_LFF_WelcomeFade.png"))
    for name, calm in (("T_LFF_CalmActive_UI", False), ("T_LFF_CalmQuiet_UI", True)):
        toggle_scene(calm).save(os.path.join(OUT, name + ".png"))
    print("Prepared Five Below logo, welcome fade and illustrated Calm Mode scenes.")


if __name__ == "__main__":
    main()
