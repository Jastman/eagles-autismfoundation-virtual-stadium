"""The Sensory Activation Vehicle's side wrap, from the reference photo of the real trailer.

  Assets/sensoryVehicle.gltf (its wrap texture, image 1)  +  Assets/Reference/SensoryVehicle_Side.jpg
      ->  Saved/PropImport/T_LFF_SAV_Wrap.jpg

Both long sides of the model's hull map one strip of the wrap texture (u 0.167-0.866, v 0.370-0.664; the rear at the
left, the hull's bottom edge at the bottom, the curb side mirrored in the mesh so it reads too). The photo is a near
side-on shot: its wrap (rear edge, roof trim line, front edge, bottom trim) is rectified into that strip, so the side
reads like the real trailer: the lettering, the green MOBILE SENSORY SPACE stripe, the KultureCity, CHOP and Eagles
Autism Foundation logos and the door with its EAF panel. The photo's fender and wheel tops are painted over with the
wrap's own topo pattern (from the model's texture), since the model has its fenders and wheels in 3D in the same
place (15-62 % of the length on both). The rest of the texture (roof, ends) is the model's.
The photo door is registered to the model's physical frame, so the baked and geometric edges do not appear twice.
Import with Tools/Editor/import_lff_props.py (it uses this file when it's there). Needs Pillow and numpy.

  python3 Tools/Pipeline/prepare_sav_wrap.py
"""
import base64
import io
import json
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
GLTF = os.path.join(ROOT, "Assets", "sensoryVehicle.gltf")
PHOTO = os.path.join(ROOT, "Assets", "Reference", "SensoryVehicle_Side.jpg")
OUT = os.path.join(ROOT, "Saved", "PropImport", "T_LFF_SAV_Wrap.jpg")

# The side strip in the model's 2048 px texture (from the hull's UVs).
STRIP = (342, 758, 1774, 1360)  # left, top, right, bottom
# The wrap's corners in the photo (2000 x 1333): rear top, rear bottom, front bottom, front top. The tops sit on the
# roof trim line, the bottoms under the white bottom trim; the front edge includes the diamond-plate stone guard.
WRAP = ((134, 179), (139, 1005), (1848, 1003), (1848, 186))
# The photo's fender (and the wheel tops under it), in the photo, above the bottom trim: painted over.
FENDER = (396, 820, 1194, 986)
TRIM_TOP = 988  # the white bottom trim's top edge in the photo
# A patch of plain topo pattern in the model's texture, below the strip (lines only, no lettering or logos).
TOPO = (342, 1400, 1774, 1980)
# Outer door frame in the photo and its measured projection onto the hull's atlas.
# The latter includes the door01/hull node translations and the curb-side UV mapping.
PHOTO_DOOR = (1258, 214, 1593, 983)
ATLAS_DOOR = (1329.535, 789.541, 1587.044, 1346.156)


def model_texture():
    with open(GLTF) as f:
        doc = json.load(f)
    uri = doc["images"][1]["uri"]
    return Image.open(io.BytesIO(base64.b64decode(uri.split(",", 1)[1]))).convert("RGB")


def photo_to_strip(point, size):
    corners = np.asarray(WRAP, dtype=np.float64)
    origin = corners[0]
    horizontal = corners[3] - origin
    vertical = corners[1] - origin
    cross = corners[2] - corners[1] - corners[3] + origin
    target = np.asarray(point, dtype=np.float64) - origin
    uv = np.linalg.solve(np.column_stack((horizontal, vertical)), target)
    # Invert the same bilinear QUAD mapping used by Pillow, including the slight photo skew.
    for _ in range(5):
        residual = horizontal * uv[0] + vertical * uv[1] + cross * uv.prod() - target
        jacobian = np.column_stack((horizontal + cross * uv[1], vertical + cross * uv[0]))
        uv -= np.linalg.solve(jacobian, residual)
    if np.max(np.abs(residual)) > 1e-6:
        raise ValueError(f"Could not project photo point {point} into the side strip")
    return uv * size


def align_door(side):
    w, h = side.size
    left, top, right, bottom = PHOTO_DOOR
    source = np.array([
        photo_to_strip((left, top), (w, h)),
        photo_to_strip((right, top), (w, h)),
        photo_to_strip((right, bottom), (w, h)),
        photo_to_strip((left, bottom), (w, h)),
    ])
    source_x = [0, source[[0, 3], 0].mean(), source[[1, 2], 0].mean(), w]
    source_y = [0, source[:2, 1].mean(), source[2:, 1].mean(), h]
    target_x = [0, round(ATLAS_DOOR[0] - STRIP[0]), round(ATLAS_DOOR[2] - STRIP[0]), w]
    target_y = [0, round(ATLAS_DOOR[1] - STRIP[1]), round(ATLAS_DOOR[3] - STRIP[1]), h]
    mesh = []
    for x in range(3):
        for y in range(3):
            box = (target_x[x], target_y[y], target_x[x + 1], target_y[y + 1])
            quad = (source_x[x], source_y[y], source_x[x], source_y[y + 1],
                    source_x[x + 1], source_y[y + 1], source_x[x + 1], source_y[y])
            mesh.append((box, quad))
    return side.transform(side.size, Image.Transform.MESH, mesh, Image.Resampling.BICUBIC)


def main():
    tex = model_texture()
    photo = Image.open(PHOTO).convert("RGB")
    # The real trailer's wider drip cap has no counterpart in the model's rectangular frame.
    photo.paste(photo.crop((1240, 185, 1606, 196)).resize((366, 19)), (1240, 196))
    w, h = STRIP[2] - STRIP[0], STRIP[3] - STRIP[1]
    quad = [c for corner in WRAP for c in corner]
    side = photo.transform((w * 2, h * 2), Image.QUAD, quad, Image.BICUBIC).resize((w, h), Image.LANCZOS)

    # Fender: map the photo box into the strip, fill it from the topo patch, feathered so no seam shows.
    (x0, y0), (_, y1) = WRAP[0], WRAP[1]
    sx, sy = w / (WRAP[2][0] - x0), h / (y1 - y0)
    fx0, fy0 = round((FENDER[0] - x0) * sx), round((FENDER[1] - y0) * sy)
    fx1, fy1 = round((FENDER[2] - x0) * sx), round((FENDER[3] - y0) * sy)
    topo = tex.crop(TOPO).resize((w, h), Image.LANCZOS)
    # Tone the patch to the photo: its black panel is lifted a little by daylight and its lines are a soft grey.
    ref = np.asarray(side.crop((fx0, fy0 - 70, fx1, fy0 - 4)).convert("L"), dtype=np.float32)  # the panel above it
    dark = np.percentile(ref, 10)  # its darker reflections; the patch sits mostly behind the 3D fender
    light = dark + 0.45 * (np.percentile(ref, 99) - dark)  # the lines are thin and soft, not lettering-white
    lum = np.asarray(topo.convert("L"), dtype=np.float32) / 255.0
    pat = dark + lum * (light - dark)
    patch = Image.fromarray(np.clip(pat, 0, 255).astype(np.uint8)).convert("RGB")
    # Down to the strip's bottom edge (under the photo's fender are the wheel tops), then the bottom trim redrawn
    # across it from the trim just behind the fender.
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rectangle((fx0, fy0, fx1, h), fill=255)
    side.paste(patch, (0, 0), mask.filter(ImageFilter.GaussianBlur(3)))
    trim_top = round((TRIM_TOP - y0) * sy)
    trim = side.crop((fx0 - 40, trim_top, fx0 - 10, h)).resize((fx1 - fx0 + 20, h - trim_top))
    side.paste(trim, (fx0 - 10, trim_top))

    side = align_door(side)
    tex.paste(side, STRIP[:2])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    tex.save(OUT, quality=93)
    print(f"{OUT}: side strip {w}x{h} from the photo, door aligned to its physical frame, "
          f"fender {fx1 - fx0}x{fy1 - fy0} px painted over")


if __name__ == "__main__":
    main()
