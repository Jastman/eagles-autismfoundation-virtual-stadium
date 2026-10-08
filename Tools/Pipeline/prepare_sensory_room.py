"""Prepare static sensory-room artwork and surface textures from the local reference photos.

Run locally, then run Tools/Editor/refine_sensory_spaces.py in the open Unreal Editor.
The reconstruction is illustrative; the photos do not establish surveyed dimensions.
"""
import base64
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved" / "SensoryRoomImport"


def bean_bag():
    rings, segments = 32, 48
    vertices, uvs = [], []
    for i in range(rings + 1):
        theta = np.pi * i / rings
        t = (1 - np.cos(theta)) / 2
        for j in range(segments + 1):
            phi = 2 * np.pi * j / segments
            fold = 1 + 0.025 * np.sin(9 * phi + 4 * theta) * np.sin(theta) ** 2
            radius = 0.64 * np.sin(theta) * (1 - 0.25 * t) * fold
            vertices.append((radius * np.cos(phi), max(0.025, 0.94*t),
                             radius * 0.91 * np.sin(phi) + 0.14*t**3))
            uvs.append((j / segments, 1 - i / rings))
    indices = []
    for i in range(rings):
        for j in range(segments):
            a = i * (segments + 1) + j
            b, c, d = a + 1, a + segments + 1, a + segments + 2
            indices.extend((a, c, b, b, c, d))
    positions = np.asarray(vertices, dtype="<f4")
    normals = np.zeros_like(positions)
    triangles = np.asarray(indices).reshape(-1, 3)
    for a, b, c in triangles:
        normal = np.cross(positions[b]-positions[a], positions[c]-positions[a])
        normals[[a, b, c]] += normal
    lengths = np.linalg.norm(normals, axis=1)
    normals /= np.maximum(lengths[:, None], 1e-8)
    normals[lengths < 1e-8] = (0, 1, 0)
    blocks = [positions.tobytes(), normals.astype("<f4").tobytes(),
              np.asarray(uvs, dtype="<f4").tobytes(), np.asarray(indices, dtype="<u2").tobytes()]
    views, offset = [], 0
    for block in blocks:
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(block)})
        offset += len(block)
    doc = {
        "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "SM_LFF_SensoryBeanBag"}],
        "meshes": [{"name": "SM_LFF_SensoryBeanBag", "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [{"name": "BeanBagFabric", "pbrMetallicRoughness": {
            "baseColorFactor": [0.15, 0.15, 0.15, 1], "metallicFactor": 0, "roughnessFactor": 0.95}}],
        "buffers": [{"byteLength": offset, "uri": "data:application/octet-stream;base64," +
                     base64.b64encode(b"".join(blocks)).decode()}], "bufferViews": views,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(vertices), "type": "VEC3",
             "min": positions.min(axis=0).tolist(), "max": positions.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": len(vertices), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(vertices), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
    }
    (OUT / "SM_LFF_SensoryBeanBag.gltf").write_text(json.dumps(doc))


def rectify(photo, corners, size, name):
    image = Image.open(ROOT / "Assets" / "Reference" / photo).convert("RGB")
    quad = [coordinate for corner in corners for coordinate in corner]
    image = image.transform(size, Image.Transform.QUAD, quad, Image.Resampling.BICUBIC)
    image.save(OUT / f"T_LFF_Sensory_{name}.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rectify("SensoryRoom_Corner.jpg", ((282, 436), (298, 746), (885, 694), (868, 447)),
            (1536, 768), "Field")
    rectify("SensoryRoom_Corner.jpg", ((1467, 406), (1460, 581), (1769, 608), (1785, 389)),
            (1024, 768), "Eagle")
    rectify("SensoryRoom_FieldWall.jpg", ((449, 480), (450, 645), (538, 636), (536, 482)),
            (256, 512), "Touch")
    rng = np.random.default_rng(47)
    y, x = np.mgrid[:512, :512]
    noise = rng.normal(0, 3, (512, 512))
    carpet = 53 + noise + 4 * np.sin(x * 1.4) + 2 * np.sin(y * 1.7)
    carpet -= ((x % 128 < 2) | (y % 128 < 2)) * 9
    image = np.stack((carpet * 1.10, carpet * 1.02, carpet * 0.90), axis=-1)
    Image.fromarray(np.clip(image, 0, 255).astype(np.uint8)).save(OUT / "T_LFF_Sensory_Carpet.png")
    fabric = 108 + rng.normal(0, 3, (512, 512)) + 2 * ((x + y) % 3 == 0)
    Image.fromarray(np.repeat(np.clip(fabric, 0, 255).astype(np.uint8)[..., None], 3, axis=2)).save(
        OUT / "T_LFF_Sensory_Fabric.png")
    bubble = Image.new("RGB", (256, 512), (6, 69, 39))
    draw = ImageDraw.Draw(bubble)
    for column in range(8):
        cx = 16 + column * 32
        draw.line((cx, 0, cx, 511), fill=(40, 153, 89), width=2)
        for _ in range(23):
            cy = int(rng.integers(8, 504))
            r = int(rng.integers(2, 5))
            bx = cx + int(rng.integers(-10, 11))
            draw.ellipse((bx-r, cy-r, bx+r, cy+r), outline=(125, 206, 152), width=1)
    bubble.save(OUT / "T_LFF_Sensory_Bubbles.png")
    bean_bag()
    manifest = {"textures": [p.name for p in sorted(OUT.glob("T_LFF_Sensory_*.png"))],
                "reference": "Illustrative reconstruction from three project-supplied photos; no animation."}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared {len(manifest['textures'])} textures in {OUT}")


if __name__ == "__main__":
    main()
