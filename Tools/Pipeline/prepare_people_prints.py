"""Print masks for the people's tees (the MetaHuman Crowd Sample tee material's PrintGraphicMap: red, green and blue
take PrintGraphicColorA, B and C, alpha is where the print covers the tee). The map spans the tee's whole UV layout
(CA_TshirtLngSlv): the front chest is around u 0.66, v 0.62 and the back around u 0.25, v 0.62 (found with a test grid).
Writes SourceData/People/:
  T_LFF_Print_EaglesHead.png  the Eagles head from Assets/Clothing Textures (white, silver, outline) on the chest
  T_LFF_Print_Staff.png       "STAFF" on the chest and, larger, across the back
Run: python3 Tools/Pipeline/prepare_people_prints.py   (Tools/Editor/build_lff_standins.py imports them)
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "SourceData", "People")
SIZE = 1024
FRONT, BACK = (0.66, 0.62), (0.25, 0.60)  # UV centres of the chest and the upper back


def place(canvas, art, centre, width):
    """Pastes RGBA art onto the canvas, `width` of the UV space wide, centred at `centre` (u, v)."""
    k = SIZE * width / art.size[0]
    art = art.resize((round(art.size[0] * k), round(art.size[1] * k)), Image.LANCZOS)
    canvas.alpha_composite(art, (round(SIZE * centre[0] - art.size[0] / 2), round(SIZE * centre[1] - art.size[1] / 2)))


def eagles_head():
    src = Image.open(os.path.join(ROOT, "Assets", "Clothing Textures", "Eagles Logo - Head and Eagles.png")).convert("RGBA")
    w, h = src.size
    head = src.crop((0, 0, w, int(h * 0.62)))  # the head only, not the wordmark
    head = head.crop(head.getchannel("A").getbbox())
    px = head.load()
    mask = Image.new("RGBA", head.size, (0, 0, 0, 0))
    mp = mask.load()
    for y in range(head.size[1]):
        for x in range(head.size[0]):
            r, g, b, a = px[x, y]
            if a < 128:
                continue
            if r > 225 and g > 225 and b > 225:
                mp[x, y] = (255, 0, 0, 255)  # white feathers
            elif abs(r - g) < 18 and abs(g - b) < 18 and (r + g + b) / 3 > 110:
                mp[x, y] = (0, 255, 0, 255)  # silver
            else:
                mp[x, y] = (0, 0, 255, 255)  # the dark outline
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    place(canvas, mask, FRONT, 0.13)
    canvas.save(os.path.join(OUT, "T_LFF_Print_EaglesHead.png"))


def staff():
    text = Image.new("RGBA", (1600, 400), (0, 0, 0, 0))
    d = ImageDraw.Draw(text)
    font = None
    for path in ("/System/Library/Fonts/Supplemental/Arial Black.ttf", "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                 "/Library/Fonts/Arial Bold.ttf"):
        if os.path.exists(path):
            font = ImageFont.truetype(path, 300)
            break
    font = font or ImageFont.load_default()
    box = d.textbbox((0, 0), "STAFF", font=font)
    tw, th = box[2] - box[0], box[3] - box[1]
    d.text(((1600 - tw) / 2 - box[0], (400 - th) / 2 - box[1]), "STAFF", fill=(255, 0, 0, 255), font=font)
    text = text.crop(text.getchannel("A").getbbox())
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    place(canvas, text, FRONT, 0.12)
    place(canvas, text, BACK, 0.2)
    canvas.save(os.path.join(OUT, "T_LFF_Print_Staff.png"))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    eagles_head()
    staff()
    print("wrote", sorted(os.listdir(OUT)))
